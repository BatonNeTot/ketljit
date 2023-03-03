﻿/*🍲Ketl🍲*/
#ifndef compiler_common_h
#define compiler_common_h

#include "ketl.h"
#include "type.h"

namespace Ketl {

	class Context;

	enum class OperatorCode : uint8_t {
		None,
		Constructor,
		Destructor,
		Plus,
		Minus,
		Multiply,
		Divide,
		Assign,
		
	};

	inline OperatorCode parseOperatorCode(const std::string_view& opStr) {
		switch (opStr.length()) {
		case 1: {
			switch (opStr.data()[0]) {
			case '+': return OperatorCode::Plus;
			case '-': return OperatorCode::Minus;
			case '*': return OperatorCode::Multiply;
			case '/': return OperatorCode::Divide;
			case '=': return OperatorCode::Assign;
			}
		}
		}

		return OperatorCode::None;
	}

	class TypeTemplate {
	public:
		virtual ~TypeTemplate() = default;
	};

	class SemanticAnalyzer;

	class AnalyzerVar {
	public:
		virtual ~AnalyzerVar() = default;
		virtual std::pair<Argument::Type, Argument> getArgument(SemanticAnalyzer& context) const = 0;
	};

	class RawInstruction {
	public:
		Instruction::Code code = Instruction::Code::None;
		AnalyzerVar* outputVar;
		AnalyzerVar* firstVar;
		AnalyzerVar* secondVar;

		void propagadeInstruction(Instruction& instruction, SemanticAnalyzer& context);
	};

	// Intermediate representation node
	class IRNode {
	public:

		virtual ~IRNode() = default;
		virtual AnalyzerVar* produceInstructions(std::vector<RawInstruction>& instructions, SemanticAnalyzer& context)&& { return {}; };
		virtual const std::string& id() const {
			static const std::string empty;
			return empty;
		}
		virtual const std::shared_ptr<TypeTemplate>& type() const { 
			static const std::shared_ptr<TypeTemplate> empty;
			return empty; 
		};
	};

}

#endif /*compiler_common_h*/