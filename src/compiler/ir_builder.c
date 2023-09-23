//🍲ketl
#include "ir_builder.h"

#include "ir_node.h"
#include "ketl/compiler/syntax_node.h"

#include "ketl/object_pool.h"
#include "ketl/atomic_strings.h"
#include "ketl/type.h"
#include "ketl/ketl.h"
#include "ketl/assert.h"

#include <stdlib.h>

KETL_FORWARD(KETLType);

KETL_DEFINE(IRUndefinedValue) {
	KETLIRVariable* variable;
	uint64_t scopeIndex;
	IRUndefinedValue* next;
};

KETL_DEFINE(IRUndefinedDelegate) {
	IRUndefinedValue* caller;
	IRUndefinedValue* arguments;
	IRUndefinedValue* next;
};

KETL_DEFINE(CastingOption) {
	KETLCastOperator* operator;
	KETLIRVariable* variable;
	uint64_t score;
	CastingOption* next;
};

void ketlInitIRBuilder(KETLIRBuilder* irBuilder, KETLState* state) {
	irBuilder->state = state;

	ketlInitObjectPool(&irBuilder->variablesPool, sizeof(KETLIRVariable), 16);
	ketlInitObjectPool(&irBuilder->operationsPool, sizeof(KETLIROperation), 16);
	ketlInitObjectPool(&irBuilder->udelegatePool, sizeof(IRUndefinedDelegate), 16);
	ketlInitObjectPool(&irBuilder->uvaluePool, sizeof(IRUndefinedValue), 16);
	ketlInitObjectPool(&irBuilder->argumentPointersPool, sizeof(KETLIRArgument*), 32);
	ketlInitObjectPool(&irBuilder->argumentsPool, sizeof(KETLIRArgument), 16);

	ketlInitIntMap(&irBuilder->operationReferMap, sizeof(uint64_t), 16);
	ketlInitIntMap(&irBuilder->argumentsMap, sizeof(uint64_t), 16);

	//KETLObjectPool castingPool;
}

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder) {
	ketlDeinitIntMap(&irBuilder->argumentsMap);
	ketlDeinitIntMap(&irBuilder->operationReferMap);

	ketlDeinitObjectPool(&irBuilder->argumentsPool);
	ketlDeinitObjectPool(&irBuilder->argumentPointersPool);
	ketlDeinitObjectPool(&irBuilder->uvaluePool);
	ketlDeinitObjectPool(&irBuilder->udelegatePool);
	ketlDeinitObjectPool(&irBuilder->operationsPool);
	ketlDeinitObjectPool(&irBuilder->variablesPool);
}


static inline IRUndefinedDelegate* wrapInDelegateUValue(KETLIRFunctionWIP* wip, IRUndefinedValue* uvalue) {
	IRUndefinedDelegate* udelegate = ketlGetFreeObjectFromPool(&wip->builder->udelegatePool);
	udelegate->caller = uvalue;
	udelegate->arguments = NULL;
	udelegate->next = NULL;

	return udelegate;
}

static inline IRUndefinedValue* wrapInUValueVariable(KETLIRFunctionWIP* wip, KETLIRVariable* variable) {
	IRUndefinedValue* uvalue = ketlGetFreeObjectFromPool(&wip->builder->uvaluePool);
	uvalue->variable = variable;
	uvalue->next = NULL;

	return uvalue;
}

static inline IRUndefinedDelegate* wrapInDelegateValue(KETLIRFunctionWIP* wip, KETLIRVariable* variable) {
	return wrapInDelegateUValue(wip, wrapInUValueVariable(wip, variable));
}

static inline KETLIROperation* createOperation(KETLIRFunctionWIP* wip) {
	return ketlGetFreeObjectFromPool(&wip->builder->operationsPool);
}

/*
static inline KETLIRValue* createTempVariable(KETLIRFunctionWIP* wip) {
	KETLIRValue* stackValue = ketlGetFreeObjectFromPool(&wip->builder->valuePool);
	stackValue->name = NULL;
	stackValue->argTraits.type = KETL_INSTRUCTION_ARGUMENT_TYPE_STACK;
	stackValue->argument.stack = 0;
	stackValue->firstChild = NULL;
	stackValue->nextSibling = NULL;

	KETLIRValue* tempVariables = wip->tempVariables;
	if (tempVariables != NULL) {
		stackValue->parent = tempVariables;
		tempVariables->firstChild = stackValue;
		wip->tempVariables = stackValue;
		return stackValue;
	}
	else {
		stackValue->parent = NULL;
		wip->tempVariables = stackValue;
		return stackValue;
	}
}

static inline KETLIRValue* createLocalVariable(KETLIRFunctionWIP* wip) {
	KETLIRValue* stackValue = ketlGetFreeObjectFromPool(&wip->builder->valuePool);
	stackValue->argTraits.type = KETL_INSTRUCTION_ARGUMENT_TYPE_STACK;
	stackValue->argument.stack = 0;
	stackValue->firstChild = NULL;
	stackValue->nextSibling = NULL;

	KETLIRValue* localVariables = wip->localVariables;
	if (localVariables != NULL) {
		stackValue->parent = localVariables;
		localVariables->firstChild = stackValue;
		wip->localVariables = stackValue;
		return stackValue;
	}
	else {
		stackValue->parent = NULL;
		wip->localVariables = stackValue;
		return stackValue;
	}
}
*/

KETL_DEFINE(Literal) {
	KETLType* type;
	KETLIRArgument value;
};

static inline Literal parseLiteral(KETLIRFunctionWIP* wip, const char* value, size_t length) {
	Literal literal;
	int64_t intValue = ketlStrToI64(value, length);
	if (INT8_MIN <= intValue && intValue <= INT8_MAX) {
		literal.type = wip->builder->state->primitives.i8_t;
		literal.value.int8 = (int8_t)intValue;
		literal.value.type = KETL_IR_ARGUMENT_TYPE_INT8;
	}
	else if (INT16_MIN <= intValue && intValue <= INT16_MAX) {
		literal.type = wip->builder->state->primitives.i16_t;
		literal.value.int16 = (int16_t)intValue;
		literal.value.type = KETL_IR_ARGUMENT_TYPE_INT16;
	}
	else if (INT32_MIN <= intValue && intValue <= INT32_MAX) {
		literal.type = wip->builder->state->primitives.i32_t;
		literal.value.int32 = (int32_t)intValue;
		literal.value.type = KETL_IR_ARGUMENT_TYPE_INT32;
	}
	else if (INT64_MIN <= intValue && intValue <= INT16_MAX) {
		literal.type = wip->builder->state->primitives.i64_t;
		literal.value.int64 = intValue;
		literal.value.type = KETL_IR_ARGUMENT_TYPE_INT64;
	}
	return literal;
}

KETL_DEFINE(IRResult) {
	IRUndefinedDelegate* delegate;
	KETLIROperation* nextOperation;
};

static IRResult buildIRFromSyntaxNode(KETLIRFunctionWIP* wip, KETLIROperation* rootOperation, KETLSyntaxNode* syntaxNodeRoot) {
	IRResult result;
	result.delegate = NULL;
	result.nextOperation = NULL;

	KETLSyntaxNode* it = syntaxNodeRoot;
	switch (it->type) {
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR: {
		/*
		KETLSyntaxNode* idNode = it->firstChild;
		KETLIRValue* variable = createVariableDefinition(irBuilder, wip, idNode, NULL);

		if (variable == NULL) {
			return NULL;
		}

		return wrapInDelegateValue(irBuilder, variable);
		*/
	}
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE: {
		/*
		KETLSyntaxNode* typeNode = it->firstChild;

		KETLState* state = wip->builder->state;
		const char* typeName = ketlAtomicStringsGet(&state->strings, typeNode->value, typeNode->length);

		KETLType* type = ketlIntMapGet(&state->globalNamespace.types, (KETLIntMapKey)typeName);

		KETLSyntaxNode* idNode = typeNode->nextSibling;
		KETLIRValue* variable = createVariableDefinition(irBuilder, wip, idNode, type);

		if (variable == NULL) {
			return NULL;
		}

		return wrapInDelegateValue(irBuilder, variable);
		*/
	}
	case KETL_SYNTAX_NODE_TYPE_ID: {
		/*
		const char* uniqName = ketlAtomicStringsGet(&wip->builder->state->strings, it->value, it->length);
		IRUndefinedValue** ppValue;
		if (ketlIntMapGetOrCreate(&wip->builder->variables, (KETLIntMapKey)uniqName, &ppValue)) {
			// TODO error
			__debugbreak();
		}
		return wrapInDelegateUValue(irBuilder, *ppValue);
		*/
	}
	case KETL_SYNTAX_NODE_TYPE_NUMBER: {
		Literal literal = parseLiteral(wip, it->value, it->length);
		if (literal.type == NULL) {
			// TODO error
		}
		KETLIRVariable* variable = ketlGetFreeObjectFromPool(&wip->builder->variablesPool);
		variable->type = literal.type;
		variable->traits.isNullable = false;
		variable->traits.isConst = true;
		variable->traits.type = KETL_TRAIT_TYPE_LITERAL;
		variable->value = literal.value;

		result.delegate = wrapInDelegateValue(wip, variable);
		result.nextOperation = rootOperation;

		return result;
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PROD:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_DIV:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_EQUAL:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_UNEQUAL: {
		/*
		KETLIRValue* result = createTempVariable(irBuilder, wip);

		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRFromSyntaxNode(irBuilder, wip, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRFromSyntaxNode(irBuilder, wip, rhsNode);

		ketlResetPool(&wip->builder->castingPool);
		BinaryOperatorDeduction deductionStruct = deduceInstructionCode2(irBuilder, it->type, lhs, rhs);
		if (deductionStruct.operator == NULL) {
			// TODO error
			__debugbreak();
		}

		convertValues(irBuilder, wip, deductionStruct.lhsCasting);
		convertValues(irBuilder, wip, deductionStruct.rhsCasting);

		KETLBinaryOperator deduction = *deductionStruct.operator;
		KETLIRInstruction* instruction = getOperation(irBuilder, wip);
		instruction->code = deduction.code;
		instruction->arguments[1] = deductionStruct.lhsCasting->value;
		instruction->arguments[2] = deductionStruct.rhsCasting->value;

		instruction->arguments[0] = result;
		result->type = deduction.outputType;
		result->traits = deduction.outputTraits;

		return wrapInDelegateValue(irBuilder, result);
		*/
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN: {
		/*
		KETLSyntaxNode* lhsNode = it->firstChild;
		IRResult lhs = buildIRFromSyntaxNode(wip, refer, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRResult rhs = buildIRFromSyntaxNode(wip, lhs.nextInstructionRefer, rhsNode);

		KETLIROperation* operation = getOperation(wip, rhs.nextInstructionRefer);

		operation->code = KETL_INSTRUCTION_CODE_ASSIGN_8_BYTES; // TODO choose from type

		operation->arguments[0] = lhs.delegate->caller->value; // TODO actual convertion from udelegate to correct type
		operation->arguments[1] = rhs.delegate->caller->value; // TODO actual convertion from udelegate to correct type

		result.delegate = wrapInDelegateValue(wip, operation->arguments[0]);
		result.nextInstructionRefer = claimOperation(wip);

		operation->mainNext = result.nextInstructionRefer;

		return result;
		*/
	}
	case KETL_SYNTAX_NODE_TYPE_RETURN: {
		KETLSyntaxNode* expressionNode = it->firstChild;
		if (expressionNode) {
			IRResult expression = buildIRFromSyntaxNode(wip, rootOperation, expressionNode);

			KETLIROperation* assignOperation = expression.nextOperation;
			assignOperation->code = KETL_IR_CODE_ASSIGN_8_BYTES; // TODO deside from type
			assignOperation->argumentCount = 2;
			assignOperation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 2);
			assignOperation->arguments[0] = ketlGetFreeObjectFromPool(&wip->builder->argumentsPool);
			assignOperation->arguments[0]->type = KETL_IR_ARGUMENT_TYPE_RETURN;
			assignOperation->arguments[1] = &expression.delegate->caller->variable->value; // TODO actual convertion from udelegate to correct type

			rootOperation = createOperation(wip);

			assignOperation->mainNext = rootOperation;
		}

		KETLIROperation* operation = rootOperation;
		operation->code = KETL_IR_CODE_RETURN;
		operation->argumentCount = 0;

		return result;
	}
	KETL_NODEFAULT();
	__debugbreak();
	}
	return result;
}


static void buildIRBlock(KETLIRFunctionWIP* wip, KETLIROperation* rootOperation, KETLSyntaxNode* syntaxNode) {
	for (KETLSyntaxNode* it = syntaxNode; it; it = it->nextSibling) {
		switch (it->type) {
		case KETL_SYNTAX_NODE_TYPE_BLOCK: {

			//KETLIRValue* savedStack = wip->builder->currentStack;
			//uint64_t scopeIndex = wip->builder->scopeIndex++;

			buildIRBlock(wip, rootOperation, it->firstChild);

			//restoreScopeContext(irBuilder, savedStack, scopeIndex);
			break;
		}
		case KETL_SYNTAX_NODE_TYPE_IF_ELSE: {
			/*
			KETLIRValue* savedStack = wip->builder->currentStack;
			uint64_t scopeIndex = wip->builder->scopeIndex++;

			KETLIRValue* currentStack = wip->builder->currentStack;
			KETLIRValue* stackRoot = wip->builder->stackRoot;

			wip->builder->tempVariables = NULL;
			wip->builder->localVariables = NULL;

			KETLSyntaxNode* expressionNode = it->firstChild;
			IRUndefinedDelegate* expression = buildIRFromSyntaxNode(irBuilder, expressionNode);

			CastingOption* expressionCasting = castDelegateToVariable(irBuilder, expression, wip->builder->state->primitives.bool_t);
			if (expressionCasting == NULL) {
				// TODO error
				__debugbreak();
			}
			convertValues(irBuilder, expressionCasting);

			restoreLocalSopeContext(irBuilder, currentStack, stackRoot);

			KETLIRInstruction* ifJumpInstruction = getOperation(irBuilder);
			ifJumpInstruction->code = KETL_INSTRUCTION_CODE_JUMP_IF_FALSE;
			ifJumpInstruction->arguments[0] = createJumpLiteral(irBuilder);
			ifJumpInstruction->arguments[1] = expressionCasting->value;

			KETLSyntaxNode* trueBlockNode = expressionNode->nextSibling;
			// builds all instructions
			buildIRBlock(irBuilder, trueBlockNode);

			if (trueBlockNode->nextSibling != NULL) {
				KETLIRInstruction* jumpInstruction = getOperation(irBuilder);
				jumpInstruction->code = KETL_INSTRUCTION_CODE_JUMP;
				jumpInstruction->arguments[0] = createJumpLiteral(irBuilder);

				KETLIRInstruction* labelAfterTrueBlockInstruction = getOperation(irBuilder);
				labelAfterTrueBlockInstruction->code = KETL_INSTRUCTION_CODE_NONE;
				ifJumpInstruction->arguments[0]->argument.globalPtr = labelAfterTrueBlockInstruction;

				KETLSyntaxNode* falseBlockNode = trueBlockNode->nextSibling;
				// builds all instructions
				buildIRBlock(irBuilder, wip, falseBlockNode);

				KETLIRInstruction* labelAfterFalseBlockInstruction = getOperation(irBuilder);
				labelAfterFalseBlockInstruction->code = KETL_INSTRUCTION_CODE_NONE;
				jumpInstruction->arguments[0]->argument.globalPtr = labelAfterFalseBlockInstruction;
			}
			else {
				KETLIRInstruction* labelAfterTrueBlockInstruction = getOperation(irBuilder);
				labelAfterTrueBlockInstruction->code = KETL_INSTRUCTION_CODE_NONE;
				ifJumpInstruction->arguments[0]->argument.globalPtr = labelAfterTrueBlockInstruction;

			}

			restoreScopeContext(irBuilder, savedStack, scopeIndex);
			*/
			break;
		}
		default: {
			//KETLIRValue* currentStack = wip->builder->currentStack;
			//KETLIRValue* stackRoot = wip->builder->stackRoot;

			//wip->builder->tempVariables = NULL;
			//wip->builder->localVariables = NULL;

			buildIRFromSyntaxNode(wip, rootOperation, it);

			//restoreLocalSopeContext(irBuilder, currentStack, stackRoot); 
		}
		}
	}
}
static void countOperationsAndArguments(KETLIRFunctionWIP* wip, KETLIntMap* operationReferMap, KETLIntMap* argumentsMap, KETLIROperation* rootOperation) {
	KETL_FOREVER {
		uint64_t* newRefer;
		if (!ketlIntMapGetOrCreate(operationReferMap, (KETLIntMapKey)rootOperation, &newRefer)) {
			return;
		}
		*newRefer = ketlIntMapGetSize(operationReferMap) - 1;
		switch (rootOperation->code) {
		case KETL_IR_CODE_ASSIGN_8_BYTES: {
			uint64_t* newIndex;
			if (ketlIntMapGetOrCreate(argumentsMap, (KETLIntMapKey)rootOperation->arguments[0], &newIndex)) {
				*newIndex = ketlIntMapGetSize(argumentsMap) - 1;
			}
			if (ketlIntMapGetOrCreate(argumentsMap, (KETLIntMapKey)rootOperation->arguments[1], &newIndex)) {
				*newIndex = ketlIntMapGetSize(argumentsMap) - 1;
			}
			rootOperation = rootOperation->mainNext;
			continue;
		}
		case KETL_IR_CODE_RETURN:
			return;
		}
	}
}

KETLIRFunction* ketlBuildIR(KETLType* returnType, KETLIRBuilder* irBuilder, KETLSyntaxNode* syntaxNodeRoot) {
	KETLIRFunctionWIP wip;

	wip.builder = irBuilder;

	KETLIROperation* rootOperation = createOperation(&wip);
	wip.rootOperation = rootOperation;
	buildIRBlock(&wip, rootOperation, syntaxNodeRoot);

	KETLIntMap operationReferMap;
	ketlInitIntMap(&operationReferMap, sizeof(uint64_t), 16);
	KETLIntMap argumentsMap;
	ketlInitIntMap(&argumentsMap, sizeof(uint64_t), 16);

	countOperationsAndArguments(&wip, &operationReferMap, &argumentsMap, rootOperation);
	uint64_t operationCount = ketlIntMapGetSize(&operationReferMap);
	uint64_t argumentsCount = ketlIntMapGetSize(&argumentsMap);

	KETLIRFunctionDefinition functionDefinition;

	functionDefinition.function = malloc(sizeof(KETLIRFunction));

	functionDefinition.function->arguments = malloc(sizeof(KETLIRArgument) * argumentsCount);

	KETLIntMapIterator argumentIterator;
	ketlInitIntMapIterator(&argumentIterator, &argumentsMap);
	while (ketlIntMapIteratorHasNext(&argumentIterator)) {
		KETLIRArgument* oldArgument;
		uint64_t* argumentNewIndex;
		ketlIntMapIteratorGet(&argumentIterator, (KETLIntMapKey*)&oldArgument, &argumentNewIndex);
		functionDefinition.function->arguments[*argumentNewIndex] = *oldArgument;
		ketlIntMapIteratorNext(&argumentIterator);
	}

	functionDefinition.function->operationsCount = operationCount;
	functionDefinition.function->operations = malloc(sizeof(KETLIROperation) * operationCount);

	KETLIntMapIterator operationIterator;
	ketlInitIntMapIterator(&operationIterator, &operationReferMap);
	while (ketlIntMapIteratorHasNext(&operationIterator)) {
		KETLIROperation* pOldOperation;
		uint64_t* operationNewIndex;
		ketlIntMapIteratorGet(&operationIterator, (KETLIntMapKey*)&pOldOperation, &operationNewIndex);
		KETLIROperation oldOperation = *pOldOperation;
		KETLIRArgument* arguments = functionDefinition.function->arguments;
		for (uint64_t i = 0; i < oldOperation.argumentCount; ++i) {
			uint64_t* argumentNewIndex = ketlIntMapGet(&argumentsMap, (KETLIntMapKey)oldOperation.arguments[i]);
			oldOperation.arguments[i] = &arguments[*argumentNewIndex];
		}
		functionDefinition.function->operations[*operationNewIndex] = oldOperation;
		ketlIntMapIteratorNext(&operationIterator);
	}

	functionDefinition.type = NULL;


	return functionDefinition.function;
}