﻿/*🍲Ketl🍲*/
#include "semantic_analyzer.h"

#include "context.h"

#include <sstream>

namespace Ketl {

	bool UndeterminedVar::canBeOverloadedWith(const TypeObject& type) const {
		if (_potentialVars.empty()) {
			return true;
		}
		return _potentialVars[0].argument->getType()->doesSupportOverload() && type.doesSupportOverload();
	};

	bool InstructionSequence::verifyReturn() {
		if (_hasReturnStatement && !_raisedAfterReturnError) {
			_raisedAfterReturnError = true;
			_context.pushErrorMsg("[WARNING] Statements after return");
		}

		return !_raisedAfterReturnError;
	}

	FullInstruction* InstructionSequence::createFullInstruction() {
		auto fullInstruction = std::make_unique<FullInstruction>();
		auto fullInstructionPtr = fullInstruction.get();
		_rawInstructions.emplace_back(std::move(fullInstruction));
		return fullInstructionPtr;
	}

	class CallInstruction : public RawInstruction {
	public:
		RawArgument* caller;
		std::vector<RawArgument*> arguments;

		void propagadeInstruction(InstructionIterator  instructions) const override {
			__debugbreak();
		}
		uint64_t countInstructions() const override {
			return arguments.size() + Instruction::getCodeSize(Instruction::Code::AllocateFunctionStack) + Instruction::getCodeSize(Instruction::Code::AllocateFunctionStack); // allocate; set all arguments; call
		}
	};

	CompilerVar InstructionSequence::createDefine(const std::string_view& id, const TypeObject& type, RawArgument* expression) {
		// TODO get const and ref
		auto var = _context.createVar(id, type, { false, false });

		if (expression) {
			auto instruction = std::make_unique<FullInstruction>();

			instruction->code = Instruction::Code::Assign;
			instruction->arguments.emplace_back(_context.createLiteralVar(expression->getType()->sizeOf()).argument);
			instruction->arguments.emplace_back(expression);
			instruction->arguments.emplace_back(var.argument);

			_rawInstructions.emplace_back(std::move(instruction));
		}

		return var;
	}

	RawArgument* InstructionSequence::createFunctionCall(RawArgument* caller, std::vector<RawArgument*>&& arguments) {
		auto functionVar = caller;
		auto functionType = functionVar->getType();

		// allocating stack
		auto allocInstruction = std::make_unique<FullInstruction>();
		allocInstruction->code = Instruction::Code::AllocateFunctionStack;

		auto stackVar = _context.createTempVar(*_context.context().getVariable("UInt64").as<TypeObject>());
		allocInstruction->arguments.emplace_back(functionVar);
		allocInstruction->arguments.emplace_back(stackVar);

		_rawInstructions.emplace_back(std::move(allocInstruction));

		// evaluating arguments
		auto& parameters = functionType->getParameters();
		for (auto i = 0u; i < arguments.size(); ++i) {
			auto& argumentVar = arguments[i];
			auto& parameter = parameters[i];

			auto isRef = parameter.traits.isRef;
			if (!isRef) {
				// TODO create copy for the function call if needed, for now it's reference only
			}

			auto defineInstruction = std::make_unique<FullInstruction>();
			defineInstruction->code = Instruction::Code::DefineFuncParameter;

			defineInstruction->arguments.emplace_back(argumentVar);
			defineInstruction->arguments.emplace_back(stackVar);
			defineInstruction->arguments.emplace_back(_context.createFunctionArgumentVar(i));

			_rawInstructions.emplace_back(std::move(defineInstruction));
		}

		auto& returnType = *functionType->getReturnType();
		auto outputVar = _context.createTempVar(returnType);

		// calling the function
		auto callInstruction = std::make_unique<FullInstruction>();
		callInstruction->code = Instruction::Code::CallFunction;

		callInstruction->arguments.emplace_back(functionVar);
		callInstruction->arguments.emplace_back(stackVar);
		callInstruction->arguments.emplace_back(outputVar);

		_rawInstructions.emplace_back(std::move(callInstruction));

		return outputVar;
	}

	class IfElseInstruction : public RawInstruction {
	public:

		IfElseInstruction(SemanticAnalyzer& context)
			: _conditionSeq(context), _trueBlockSeq(context), _falseBlockSeq(context) {}

		void propagadeInstruction(InstructionIterator instructions) const override {
			_conditionSeq.propagadeInstruction(instructions);
			auto conditionSize = _conditionSeq.countInstructions();

			auto trueSize = _trueBlockSeq.countInstructions();
			auto falseSize = _falseBlockSeq.countInstructions();

			instructions += conditionSize;

			if (trueSize == 0) {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfNotZero;
				instructions->argument(1).integer = instructions.offset() + falseSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero); // plus if jump
				instructions->argumentType<1>() = Argument::Type::Literal;

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();

				instructions += Instruction::getCodeSize(Instruction::Code::JumpIfNotZero);
				_falseBlockSeq.propagadeInstruction(instructions);
			}
			else {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfZero;

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();

				if (falseSize == 0) {
					instructions->argument(1).integer = instructions.offset() + trueSize
						+ Instruction::getCodeSize(Instruction::Code::JumpIfZero);	// plus if jump
					instructions->argumentType<1>() = Argument::Type::Literal;

					instructions += Instruction::getCodeSize(Instruction::Code::JumpIfZero);
					_trueBlockSeq.propagadeInstruction(instructions);
				}
				else {
					instructions->argument(1).integer = instructions.offset() + trueSize
						+ Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump 
						+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
					instructions->argumentType<1>() = Argument::Type::Literal;

					instructions += Instruction::getCodeSize(Instruction::Code::JumpIfZero);
					_trueBlockSeq.propagadeInstruction(instructions);

					instructions += trueSize;
					instructions->argument(0).uinteger = 0;
					instructions->code() = Instruction::Code::Jump;
					instructions->argument(1).integer = instructions.offset() + falseSize
						+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
					instructions->argumentType<1>() = Argument::Type::Literal;

					_falseBlockSeq.propagadeInstruction(instructions + Instruction::getCodeSize(Instruction::Code::Jump));
				}
			}
		}
		uint64_t countInstructions() const override {
			auto conditionSize = _conditionSeq.countInstructions();
			auto trueSize = _trueBlockSeq.countInstructions();
			auto falseSize = _falseBlockSeq.countInstructions();
			auto countBase = conditionSize
				+ trueSize
				+ falseSize; // jump instruction
			if (trueSize != 0 && falseSize != 0) {
				countBase 
					+= Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump 
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
			}
			else {
				countBase
					+= Instruction::getCodeSize(Instruction::Code::JumpIfZero);	// plus if jump 
			}
			return countBase;
		}

		InstructionSequence& conditionSeq() {
			return _conditionSeq;
		}

		InstructionSequence& trueBlockSeq() {
			return _trueBlockSeq;
		}

		InstructionSequence& falseBlockSeq() {
			return _falseBlockSeq;
		}

		void setConditionVar(RawArgument* var) {
			_conditionVar = var;
		}

	private:
		InstructionSequence _conditionSeq;
		InstructionSequence _trueBlockSeq;
		InstructionSequence _falseBlockSeq;
		RawArgument* _conditionVar = nullptr;
	};

	void InstructionSequence::createIfElseBranches(const IRNode& condition, const IRNode* trueBlock, const IRNode* falseBlock) {
		auto ifElseInstruction = std::make_unique<IfElseInstruction>(_context);

		auto conditionVar = condition.produceInstructions(ifElseInstruction->conditionSeq(), _context);
		ifElseInstruction->setConditionVar(conditionVar.getUVar().getVarAsItIs().argument);

		if (trueBlock) {
			trueBlock->produceInstructions(ifElseInstruction->trueBlockSeq(), _context);
		}
		if (falseBlock) {
			falseBlock->produceInstructions(ifElseInstruction->falseBlockSeq(), _context);
		}

		_rawInstructions.emplace_back(std::move(ifElseInstruction));
	}

	class WhileElseInstruction : public RawInstruction {
	public:

		WhileElseInstruction(SemanticAnalyzer& context)
			: _conditionSeq(context), _loopBlockSeq(context), _elseBlockSeq(context) {}

		void propagadeInstruction(InstructionIterator instructions) const override {
			_conditionSeq.propagadeInstruction(instructions);
			auto conditionSize = _conditionSeq.countInstructions();

			auto loopSize = _loopBlockSeq.countInstructions();
			auto elseSize = _elseBlockSeq.countInstructions();

			instructions += conditionSize;

			if (elseSize == 0) {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfZero;
				instructions->argument(1).integer = instructions.offset() + loopSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
				instructions->argumentType<1>() = Argument::Type::Literal;

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();

				instructions += Instruction::getCodeSize(Instruction::Code::JumpIfZero);

				_loopBlockSeq.propagadeInstruction(instructions);
				instructions += loopSize;

				auto backJump = -static_cast<int64_t>(loopSize + conditionSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfZero));	// plus if jump
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::Jump;
				instructions->argument(1).integer = instructions.offset() + backJump;
				instructions->argumentType<1>() = Argument::Type::Literal;
			}
			else {
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfNotZero;
				instructions->argument(1).integer = instructions.offset() + elseSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero)	// plus if jump
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
				instructions->argumentType<1>() = Argument::Type::Literal;

				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();

				instructions += Instruction::getCodeSize(Instruction::Code::JumpIfNotZero);

				_elseBlockSeq.propagadeInstruction(instructions);
				instructions += elseSize;

				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::Jump;
				instructions->argument(1).integer = instructions.offset() + loopSize + conditionSize
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero)	// plus if jump
					+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
				instructions->argumentType<1>() = Argument::Type::Literal;

				instructions += Instruction::getCodeSize(Instruction::Code::Jump); // else goto jump

				_loopBlockSeq.propagadeInstruction(instructions);
				instructions += loopSize;

				_conditionSeq.propagadeInstruction(instructions); // duplicate conditional seq
				instructions += conditionSize;

				auto backJump = -static_cast<int64_t>(loopSize + conditionSize);
				instructions->argument(0).uinteger = 0;
				instructions->code() = Instruction::Code::JumpIfNotZero;
				instructions->argument(1).integer = instructions.offset() + backJump;
				instructions->argumentType<1>() = Argument::Type::Literal;
				std::tie(instructions->argumentType<2>(), instructions->argument(2)) = _conditionVar->getArgument();
			}
		}
		uint64_t countInstructions() const override {
			auto conditionSize = _conditionSeq.countInstructions();
			auto loopSize = _loopBlockSeq.countInstructions();
			auto elseSize = _elseBlockSeq.countInstructions();
			auto countBase = conditionSize
				+ loopSize
				+ Instruction::getCodeSize(Instruction::Code::JumpIfZero)	// plus if jump
				+ Instruction::getCodeSize(Instruction::Code::Jump);		// plus goto jump
			if (elseSize != 0) {
				countBase += conditionSize // separate condition block
					+ Instruction::getCodeSize(Instruction::Code::JumpIfNotZero)	// plus if jump
					+ elseSize;
			}
			return countBase;
		}

		InstructionSequence& conditionSeq() {
			return _conditionSeq;
		}

		InstructionSequence& loopBlockSeq() {
			return _loopBlockSeq;
		}

		InstructionSequence& elseBlockSeq() {
			return _elseBlockSeq;
		}

		void setConditionVar(RawArgument* var) {
			_conditionVar = var;
		}

	private:
		InstructionSequence _conditionSeq;
		InstructionSequence _loopBlockSeq;
		InstructionSequence _elseBlockSeq;
		RawArgument* _conditionVar = nullptr;
	};

	void InstructionSequence::createWhileElseBranches(const IRNode& condition, const IRNode* loopBlock, const IRNode* elseBlock) {
		auto whileElseInstruction = std::make_unique<WhileElseInstruction>(_context);

		auto conditionVar = condition.produceInstructions(whileElseInstruction->conditionSeq(), _context);
		whileElseInstruction->setConditionVar(conditionVar.getUVar().getVarAsItIs().argument);

		if (loopBlock) {
			loopBlock->produceInstructions(whileElseInstruction->loopBlockSeq(), _context);
		}
		if (elseBlock) {
			elseBlock->produceInstructions(whileElseInstruction->elseBlockSeq(), _context);
		}

		_rawInstructions.emplace_back(std::move(whileElseInstruction));
	}

	void InstructionSequence::createReturnStatement(UndeterminedDelegate expression) {
		if (_hasReturnStatement) {
			_context.pushErrorMsg("[ERROR] Multiple return statements");
			return;
		}

		_hasReturnStatement = true;
		_returnExpression = std::move(expression);
	}

	void InstructionSequence::propagadeInstruction(InstructionIterator instructions) const {
		uint64_t offset = 0u;
		for (auto& rawInstruction : _rawInstructions) {
			rawInstruction->propagadeInstruction(instructions + offset);
			offset += rawInstruction->countInstructions();
		}
		
		if (_hasReturnStatement) {
			FullInstruction instruction;
			instruction.code = Instruction::Code::ReturnValue;
			instruction.arguments.emplace_back(_context.createLiteralVar(_returnExpression.getUVar().getVarAsItIs().argument->getType()->sizeOf()).argument);
			instruction.arguments.emplace_back(_returnExpression.getUVar().getVarAsItIs().argument);
			instruction.propagadeInstruction(instructions + offset);
		}
		else if (_mainSequence) {
			FullInstruction instruction;
			instruction.code = Instruction::Code::Return;
			instruction.propagadeInstruction(instructions + offset);
		}
	}

	SemanticAnalyzer::SemanticAnalyzer(Context& context, SemanticAnalyzer* parentContext)
		: _context(context), _localScope(parentContext != nullptr)
		, _undefinedArgument(context), _undefinedVar(&_undefinedArgument, false, {true, false}) {
		_rootLocal = &_localVars.emplace_back();
		_currentLocal = _rootLocal;

		_localsByName.emplace_back();
	}

	void SemanticAnalyzer::pushScope() {
		// TODO
	}
	void SemanticAnalyzer::popScope() {
		// TODO
	}

	RawArgument* SemanticAnalyzer::deduceUnaryOperatorCall(OperatorCode code, const UndeterminedDelegate& var, InstructionSequence& instructions) {
		// TODO
		return nullptr;
	}
	CompilerVar SemanticAnalyzer::deduceBinaryOperatorCall(OperatorCode code, const UndeterminedDelegate& lhs, const UndeterminedDelegate& rhs, InstructionSequence& instructions) {
		// TODO actual deducing
		std::string argumentsNotation = std::string("Int64,Int64");
		auto primaryOperatorPair = context().deducePrimaryOperator(code, argumentsNotation);

		if (primaryOperatorPair.first == Instruction::Code::None) {

			return CompilerVar();
		}

		auto instruction = instructions.createFullInstruction();

		instruction->code = primaryOperatorPair.first;

		RawArgument* output;
		switch (primaryOperatorPair.first) {
			case Instruction::Code::IsStructEqual:
			case Instruction::Code::IsStructNonEqual:
				output = createTempVar(*context().getVariable("Int64").as<TypeObject>());
				instruction->arguments.emplace_back(createLiteralVar(lhs.getUVar().getVarAsItIs().argument->getType()->sizeOf()).argument);
				instruction->arguments.emplace_back(lhs.getUVar().getVarAsItIs().argument);
				instruction->arguments.emplace_back(rhs.getUVar().getVarAsItIs().argument);
				instruction->arguments.emplace_back(output);
				break;
			case Instruction::Code::Assign:
				output = lhs.getUVar().getVarAsItIs().argument;
				instruction->arguments.emplace_back(createLiteralVar(lhs.getUVar().getVarAsItIs().argument->getType()->sizeOf()).argument);
				instruction->arguments.emplace_back(rhs.getUVar().getVarAsItIs().argument);
				instruction->arguments.emplace_back(output);
				break;
			default:
				output = createTempVar(*context().getVariable("Int64").as<TypeObject>());
				instruction->arguments.emplace_back(lhs.getUVar().getVarAsItIs().argument);
				instruction->arguments.emplace_back(rhs.getUVar().getVarAsItIs().argument);
				instruction->arguments.emplace_back(output);
				break;
		}

		return CompilerVar(output, false, {});
	}
	UndeterminedDelegate SemanticAnalyzer::deduceFunctionCall(const UndeterminedDelegate& caller, const std::vector<UndeterminedDelegate>& arguments, InstructionSequence& instructions) {
		auto& variants = caller.getUVar().getVariants();
		auto& delegatedArguments = caller.getArguments(); 
		
		if (variants.empty()) {
			pushErrorMsg("[ERROR] No satisfying function");
			return _undefinedVar;
		}

		std::vector<UndeterminedDelegate> totalArguments;
		totalArguments.reserve(delegatedArguments.size() + arguments.size());

		for (const auto& argument : delegatedArguments) {
			totalArguments.emplace_back(argument);
		}
		for (const auto& argument : arguments) {
			totalArguments.emplace_back(argument);
		}

		std::map<uint64_t, RawArgument*> deducedVariants;

		for (const auto& callerVar : variants) {
			auto type = callerVar.argument->getType();
			if (type->getReturnType()) {
				// it is a function
				auto& parameters = type->getParameters();

				if (totalArguments.size() > parameters.size()) {
					deducedVariants.emplace(std::make_pair<uint64_t, RawArgument*>(std::numeric_limits<uint64_t>::max(), nullptr));
					continue;
				}

				// TODO do cast checking and counting
				if (totalArguments.size() < parameters.size()) {
					deducedVariants.emplace(std::make_pair<uint64_t, RawArgument*>(0u, nullptr));
					continue;
				}

				deducedVariants.emplace(std::make_pair(0u, callerVar.argument));
				continue;
			}

			auto& callOperators = type->getCallFunctions();
			for (auto& callOperator : callOperators) {
				// TODO
			}
		}

		auto bestVariantsIt = deducedVariants.begin();
		auto bestVariantCost = bestVariantsIt->first;

		if (bestVariantCost == std::numeric_limits<uint64_t>::max()) {
			pushErrorMsg("[ERROR] No satisfying function");
			return _undefinedVar;
		}

		auto deducedEnd = deducedVariants.end();
		auto bestVariantsCount = 0u;

		for (;bestVariantsIt != deducedEnd && bestVariantsIt->first == bestVariantCost; ++bestVariantsIt) {
			if (bestVariantsIt->second != nullptr) {
				++bestVariantsCount;
			}
		}

		if (bestVariantsCount == 0) {
			// specifically a copy
			auto newDelegateCaller = caller.getUVar();
			return UndeterminedDelegate(std::move(newDelegateCaller), std::move(totalArguments));
		}

		if (bestVariantsCount > 1) {
			pushErrorMsg("[ERROR] Coundn't decide call");
			return _undefinedVar;
		}

		auto functionVar = deducedVariants.begin()->second;

		std::vector<RawArgument*> callArguments(totalArguments.size());
		for (auto i = 0u; i < totalArguments.size(); ++i) {
			callArguments[i] = totalArguments[i].getUVar().getVarAsItIs().argument;
		}

		auto outputVar = instructions.createFunctionCall(
			functionVar,
			std::move(callArguments)
		);

		return CompilerVar(outputVar, false, {});
	}
	const TypeObject* SemanticAnalyzer::deduceCommonType(const std::vector<UndeterminedDelegate>& vars) {
		// TODO
		return nullptr;
	}

	class UnsignedLiteralArgument : public RawArgument {
	public:
		UnsignedLiteralArgument(uint64_t value, SemanticAnalyzer& context)
			: _value(value) {
			_type = context.context().getVariable("Int64").as<TypeObject>();
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.uinteger = _value;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type = nullptr;
		std::uint64_t _value;
		std::string_view _id;
	};

	CompilerVar SemanticAnalyzer::createLiteralVar(uint64_t value) {
		auto ptr = vars.emplace_back(std::make_unique<UnsignedLiteralArgument>(value, *this)).get();
		return CompilerVar(ptr, true, { true, false });
	}

	class LiteralArgument : public RawArgument {
	public:
		// TODO
		LiteralArgument(const std::string_view& value, SemanticAnalyzer& context)
			: _value(value) {
			_type = context.context().getVariable("Int64").as<TypeObject>();
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.uinteger = std::stoull(std::string(_value));
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type = nullptr;
		std::string_view _value;
		std::string_view _id;
	};

	CompilerVar SemanticAnalyzer::createLiteralVar(const std::string_view& value) {
		auto ptr = vars.emplace_back(std::make_unique<LiteralArgument>(value, *this)).get();
		return CompilerVar(ptr, true, { true, false });
	}

	class LiteralClassArgument : public RawArgument {
	public:
		LiteralClassArgument(void* ptr, const TypeObject& type)
			: _ptr(ptr), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Literal;
			Argument argument;
			argument.pointer = _ptr;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		void* _ptr;
		const TypeObject* _type;
	};

	CompilerVar SemanticAnalyzer::createLiteralClassVar(void* ptr, const TypeObject& type) {
		resultRefs.emplace_back(ptr);
		auto var = vars.emplace_back(std::make_unique<LiteralClassArgument>(ptr, type)).get();

		return CompilerVar(var, true, { true, false });
	}

	class StackArgument : public RawArgument {
	public:
		StackArgument(const TypeObject& type)
			: _type(&type) {}

		void bake(void* ptr) override {
			_ptr = ptr;
		}

		inline void setOffset(uint64_t offset) {
			_offset = offset;
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			if (_ptr != nullptr) {
				auto type = Argument::Type::Global;
				Argument argument;
				argument.globalPtr = _ptr;
				return std::make_pair(type, argument);
			}
			else {
				auto type = Argument::Type::Stack;
				Argument argument;
				argument.stack = _offset;
				return std::make_pair(type, argument);
			}
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type = nullptr;
		uint64_t _offset = 0u;
		void* _ptr = nullptr;
	};

	RawArgument* SemanticAnalyzer::createTempVar(const TypeObject& type) {
		auto stackVar = std::make_unique<StackArgument>(type);
		auto stackPtr = stackVar.get();
		auto ptr = vars.emplace_back(std::move(stackVar)).get();

		auto localVar = &_localVars.emplace_back();
		localVar->argument = stackPtr;

		localVar->parent = _currentLocal->parent;
		_currentLocal->nextSibling = localVar;
		_currentLocal = localVar;

		return ptr;
	}

	RawArgument* SemanticAnalyzer::createFunctionArgumentVar(uint64_t index) {
		auto& ptr = vars.emplace_back(std::make_unique<UnsignedLiteralArgument>(index * sizeof(void*), *this));

		return ptr.get();
	}

	class NewGlobalArgument : public RawArgument {
	public:
		NewGlobalArgument(const TypeObject& type)
			: _type(&type) {}

		void bake(void* ptr) override {
			_ptr = ptr;
		}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Global;
			Argument argument;
			argument.globalPtr = _ptr;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type;
		void* _ptr = nullptr;
	};

	class GlobalArgument : public RawArgument {
	public:
		GlobalArgument(void* ptr, const TypeObject& type)
			: _ptr(ptr), _type(&type) {}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::Global;
			Argument argument;
			argument.globalPtr = _ptr;
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type;
		void* _ptr = nullptr;
	};

	UndeterminedVar SemanticAnalyzer::getVar(const std::string_view& id) {
		for (auto nameScopeIt = _localsByName.rbegin(), end = _localsByName.rend(); nameScopeIt != end; ++nameScopeIt) {
			auto nameIt = nameScopeIt->find(id);
			if (nameIt != nameScopeIt->end()) {
				return nameIt->second;
			}
		}

		auto globalIt = newGlobalVars.find(id);
		if (globalIt != newGlobalVars.end()) {
			return globalIt->second;
		}

		auto globalVar = _context.getVariable(id);
		if (!globalVar.empty()) {
			UndeterminedVar uvar;
			for (auto& var : globalVar._vars) {
				auto& ptr = vars.emplace_back(std::make_unique<GlobalArgument>(var.rawData(), var.type()));
				// TODO get const and ref from var
				uvar.overload(CompilerVar(ptr.get(), !isLocalScope(), {}));
			}
			return uvar;
		}

		auto idStr = std::string(id);
		pushErrorMsg("[ERROR] Variable '" + idStr + "' doesn't exists");
		return _undefinedVar;
	}

	CompilerVar SemanticAnalyzer::createVar(const std::string_view& id, const TypeObject& type, VarTraits traits) {
		if (isLocalScope()) {
			auto& uvar = _localsByName.back()[id];

			if (!uvar.canBeOverloadedWith(type)) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in local scope");
				return _undefinedVar;
			}

			auto tempVar = createTempVar(type);
			CompilerVar var(tempVar, false, traits);
			uvar.overload(var);

			return var;
		}
		else {
			if (!_context.getVariable(id).empty()) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in global scope");
				return _undefinedVar;
			}

			auto& uvar = newGlobalVars[id];
			if (!uvar.canBeOverloadedWith(type)) {
				pushErrorMsg("[ERROR] Variable '" + std::string(id) + "' already exists in global scope");
				return _undefinedVar;
			}

			auto ptr = vars.emplace_back(std::make_unique<NewGlobalArgument>(type)).get();
			CompilerVar var(ptr, false, traits);
			uvar.overload(var);

			return var;
		}
	}

	class ParameterArgument : public RawArgument {
	public:
		ParameterArgument(uint64_t index, const TypeObject& type)
			: _type(&type), _index(index) {}

		std::pair<Argument::Type, Argument> getArgument() const override {
			auto type = Argument::Type::FunctionParameter;
			Argument argument;
			argument.stack = _index * sizeof(void*);
			return std::make_pair(type, argument);
		}

		const TypeObject* getType() const override {
			return _type;
		}

	private:

		const TypeObject* _type;
		uint64_t _index;
	};

	CompilerVar SemanticAnalyzer::createFunctionParameterVar(uint64_t index, const std::string_view& id, const TypeObject& type, VarTraits traits) {
		auto [varIt, success] = _localsByName.back().try_emplace(id);
		if (!success) {
			pushErrorMsg("[ERROR] Parameter " + std::string(id) + " already exists");
			return _undefinedVar;
		}

		++_parametersCount;
		auto ptr = vars.emplace_back(std::make_unique<ParameterArgument>(index, type)).get();
		CompilerVar var(ptr, false, traits);
		varIt->second.overload(var);

		return var;
	}

	std::string SemanticAnalyzer::compilationErrors() const {
		std::stringstream ss;
		for (const auto& msg : _compilationErrors) {
			ss << msg << std::endl;
		}
		return ss.str();
	}

	Variable SemanticAnalyzer::evaluate(const IRNode& node) {
		// TODO evalutaion right now is quite raw

		// TODO
		// So, we need to compile and run it, BUT we need to use context which we didn't bake yet
		// what about we separate BAKE and COMPILE, so that we can compile without baking.
		// which is kinda already the thing, because baking only adds names into global name space

		return context().getVariable("Int64");
	}

	const TypeObject* SemanticAnalyzer::evaluateType(const IRNode& node) {
		
		return context().getVariable("Int64").as<TypeObject>();
	}

	void SemanticAnalyzer::bakeLocalVars() {
		std::stack<uint64_t> stackOffsets;
		uint64_t currentOffset = _parametersCount * sizeof(void*);
		_stackSize = currentOffset;
		stackOffsets.push(currentOffset);

		auto localIt = _rootLocal;
		if (localIt->firstChild) {
			while (localIt->firstChild) {
				localIt = localIt->firstChild;
				stackOffsets.push(currentOffset);
			}
		}
		else {
			localIt = localIt->nextSibling;
		}

		while (localIt) {
			// TODO alignment
			localIt->argument->setOffset(currentOffset);
			currentOffset += localIt->argument->getType()->sizeOf();

			if (currentOffset > _stackSize) {
				_stackSize = currentOffset;
			}

			if (localIt->firstChild) {
				while (localIt->firstChild) {
					localIt = localIt->firstChild;
					stackOffsets.push(currentOffset);
				}
				continue;
			}

			if (localIt->nextSibling) {
				localIt = localIt->nextSibling;
				continue;
			}

			localIt = localIt->parent;
			currentOffset = stackOffsets.top();
			stackOffsets.pop();
		}
	}

	void SemanticAnalyzer::bakeContext() {
		for (auto& var : newGlobalVars) {
			auto& variants = var.second.getVariants();
			for (auto& variant : variants) {
				auto& type = *variant.argument->getType();
				void* ptr = _context.allocateGlobal(var.first, type);
				if (ptr == nullptr) {
					std::string id(var.first);
					pushErrorMsg("[ERROR] Variable " + id + " already exists");
				}
				variant.argument->bake(ptr);
			}
		}
	}

	std::variant<FunctionObject*, std::string> SemanticAnalyzer::compile(const IRNode& block)&& {
		InstructionSequence mainSequence(*this, true);

		block.produceInstructions(mainSequence, *this);
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		bakeLocalVars();

		auto rawInstructionCount = mainSequence.countInstructions();
		auto [functionPtr, functionRefs] = context().createObject<FunctionObject>(context()._alloc, false, _stackSize, static_cast<uint32_t>(rawInstructionCount));

		bakeContext();
		if (hasCompilationErrors()) {
			return compilationErrors();
		}

		for (const auto& ref : resultRefs) {
			functionRefs->registerAbsLink(ref);
		}

		mainSequence.propagadeInstruction(functionPtr->_instructions);

		return functionPtr;
	}
}