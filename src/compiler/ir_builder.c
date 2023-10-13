﻿//🍲ketl
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

	ketlInitIntMap(&irBuilder->variablesMap, sizeof(IRUndefinedValue*), 16);
	ketlInitObjectPool(&irBuilder->scopedVariablesPool, sizeof(KETLIRScopedVariable), 16);
	ketlInitObjectPool(&irBuilder->variablesPool, sizeof(KETLIRVariable), 16);
	ketlInitObjectPool(&irBuilder->operationsPool, sizeof(KETLIROperation), 16);
	ketlInitObjectPool(&irBuilder->udelegatePool, sizeof(IRUndefinedDelegate), 16);
	ketlInitObjectPool(&irBuilder->uvaluePool, sizeof(IRUndefinedValue), 16);
	ketlInitObjectPool(&irBuilder->argumentPointersPool, sizeof(KETLIRArgument*), 32);
	ketlInitObjectPool(&irBuilder->argumentsPool, sizeof(KETLIRArgument), 16);

	ketlInitObjectPool(&irBuilder->castingPool, sizeof(CastingOption), 16);

	ketlInitIntMap(&irBuilder->operationReferMap, sizeof(uint64_t), 16);
	ketlInitIntMap(&irBuilder->argumentsMap, sizeof(uint64_t), 16);
	ketlInitStack(&irBuilder->extraNextStack, sizeof(KETLIROperation*), 16);
}

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder) {
	ketlDeinitStack(&irBuilder->extraNextStack);
	ketlDeinitIntMap(&irBuilder->argumentsMap);
	ketlDeinitIntMap(&irBuilder->operationReferMap);

	ketlDeinitObjectPool(&irBuilder->castingPool);

	ketlDeinitObjectPool(&irBuilder->argumentsPool);
	ketlDeinitObjectPool(&irBuilder->argumentPointersPool);
	ketlDeinitObjectPool(&irBuilder->uvaluePool);
	ketlDeinitObjectPool(&irBuilder->udelegatePool);
	ketlDeinitObjectPool(&irBuilder->operationsPool);
	ketlDeinitObjectPool(&irBuilder->variablesPool);
	ketlDeinitObjectPool(&irBuilder->scopedVariablesPool);
	ketlDeinitIntMap(&irBuilder->variablesMap);
}


static inline IRUndefinedDelegate* wrapInDelegateUValue(KETLIRBuilder* irBuilder, IRUndefinedValue* uvalue) {
	IRUndefinedDelegate* udelegate = ketlGetFreeObjectFromPool(&irBuilder->udelegatePool);
	udelegate->caller = uvalue;
	udelegate->arguments = NULL;
	udelegate->next = NULL;

	return udelegate;
}

static inline IRUndefinedValue* wrapInUValueVariable(KETLIRBuilder* irBuilder, KETLIRVariable* variable) {
	IRUndefinedValue* uvalue = ketlGetFreeObjectFromPool(&irBuilder->uvaluePool);
	uvalue->variable = variable;
	uvalue->next = NULL;

	return uvalue;
}

static inline IRUndefinedDelegate* wrapInDelegateValue(KETLIRBuilder* irBuilder, KETLIRVariable* variable) {
	return wrapInDelegateUValue(irBuilder, wrapInUValueVariable(irBuilder, variable));
}

static inline KETLIROperation* createOperationImpl(KETLIRBuilder* irBuilder) {
	// TODO check recycled operations
	return ketlGetFreeObjectFromPool(&irBuilder->operationsPool);
}

static inline void recycleOperationImpl(KETLIRBuilder* irBuilder, KETLIROperation* operation) {
	// TODO recycle
}

static inline void initInnerOperationRange(KETLIRBuilder* irBuilder, IROperationRange* baseRange, IROperationRange* innerRange) {
	innerRange->root = baseRange->root;
	innerRange->next = createOperationImpl(irBuilder);
}

static inline KETLIROperation* createOperationFromRange(KETLIRBuilder* irBuilder, IROperationRange* range) {
	KETLIROperation* operation = range->root;
	operation->mainNext = range->root = range->next;
	operation->extraNext = NULL;
	range->next = createOperationImpl(irBuilder);
	return operation;
}

static inline KETLIROperation* createLastOperationFromRange(KETLIRBuilder* irBuilder, IROperationRange* range) {
	KETLIROperation* operation = range->root;
	operation->extraNext = operation->mainNext = range->root = NULL;
	return operation;
}

static inline void initSideOperationRange(KETLIRBuilder* irBuilder, IROperationRange* baseRange, IROperationRange* sideRange) {
	sideRange->root = createOperationImpl(irBuilder);
	sideRange->next = baseRange->next;
}

static inline void deinitSideOperationRange(KETLIRBuilder* irBuilder, IROperationRange* sideRange) {
	recycleOperationImpl(irBuilder, sideRange->next);
}

static inline void deinitInnerOperationRange(KETLIRBuilder* irBuilder, IROperationRange* baseRange, IROperationRange* innerRange) {
	baseRange->root = innerRange->root;
	recycleOperationImpl(irBuilder, innerRange->next);
}

static inline KETLIRScopedVariable* createTempVariable(KETLIRFunctionWIP* wip) {
	KETLIRScopedVariable* stackValue = ketlGetFreeObjectFromPool(&wip->builder->scopedVariablesPool);
	stackValue->variable.value.type = KETL_IR_ARGUMENT_TYPE_STACK;
	stackValue->variable.value.stack = 0;
	stackValue->firstChild = NULL;
	stackValue->nextSibling = NULL;

	KETLIRScopedVariable* tempVariables = wip->tempVariables;
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

static inline KETLIRScopedVariable* createLocalVariable(KETLIRFunctionWIP* wip) {
	KETLIRScopedVariable* stackValue = ketlGetFreeObjectFromPool(&wip->builder->scopedVariablesPool);
	stackValue->variable.value.type = KETL_IR_ARGUMENT_TYPE_STACK;
	stackValue->variable.value.stack = 0;
	stackValue->firstChild = NULL;
	stackValue->nextSibling = NULL;

	KETLIRScopedVariable* localVariables = wip->localVariables;
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

static inline convertLiteralSize(KETLIRVariable* variable, KETLType* targetType) {
	switch (variable->type->size) {
	case 1:
		switch (targetType->size) {
		case 2: variable->value.uint16 = variable->value.uint8;
			break;
		case 4: variable->value.uint32 = variable->value.uint8;
			break;
		case 8: variable->value.uint64 = variable->value.uint8;
			break;
		}
		break;
	case 2:
		switch (targetType->size) {
			// TODO warning
		case 1: variable->value.uint8 = (uint8_t)variable->value.uint16;
			break;
		case 4: variable->value.uint32 = variable->value.uint16;
			break;
		case 8: variable->value.uint64 = variable->value.uint16;
			break;
		}
		break;
	case 4:
		switch (targetType->size) {
			// TODO warning
		case 1: variable->value.uint8 = (uint8_t)variable->value.uint32;
			break;
			// TODO warning
		case 2: variable->value.uint16 = (uint16_t)variable->value.uint32;
			break;
		case 8: variable->value.uint64 = variable->value.uint32;
			break;
		}
		break;
	case 8:
		switch (targetType->size) {
			// TODO warning
		case 1: variable->value.uint8 = (uint8_t)variable->value.uint64;
			break;
			// TODO warning
		case 2: variable->value.uint16 = (uint16_t)variable->value.uint64;
			break;
			// TODO warning
		case 4: variable->value.uint32 = (uint32_t)variable->value.uint64;
			break;
		}
		break;
	}
	variable->type = targetType;
}

static void convertValues(KETLIRFunctionWIP* wip, IROperationRange* operationRange, CastingOption* castingOption) {
	if (castingOption->variable->traits.type == KETL_TRAIT_TYPE_LITERAL) {
		convertLiteralSize(castingOption->variable,
			castingOption->operator ? castingOption->operator->outputType : castingOption->variable->type);
	}
	else {
		if (castingOption == NULL || castingOption->operator == NULL) {
			return;
		}

		KETLIRVariable* result = &createTempVariable(wip)->variable;

		KETLCastOperator casting = *castingOption->operator;
		KETLIROperation* convertOperation = createOperationFromRange(wip->builder, operationRange);
		convertOperation->code = casting.code;
		convertOperation->argumentCount = 2;
		convertOperation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 2);
		convertOperation->arguments[1] = &castingOption->variable->value;
		convertOperation->arguments[0] = &result->value;
		convertOperation->extraNext = NULL;

		result->type = casting.outputType;
		result->traits = casting.outputTraits;

		castingOption->variable = result;
	}
}

KETL_DEFINE(TypeCastingTargetList) {
	CastingOption* begin;
	CastingOption* last;
};

static TypeCastingTargetList possibleCastingForValue(KETLIRBuilder* irBuilder, KETLIRVariable* variable, bool implicit) {
	TypeCastingTargetList targets;
	targets.begin = targets.last = NULL;

	bool numberLiteral = variable->traits.type == KETL_TRAIT_TYPE_LITERAL && variable->type->kind == KETL_TYPE_KIND_PRIMITIVE;
#define MAX_SIZE_OF_NUMBER_LITERAL 8

	KETLCastOperator** castOperators = ketlIntMapGet(&irBuilder->state->castOperators, (KETLIntMapKey)variable->type);
	if (castOperators != NULL) {
		KETLCastOperator* it = *castOperators;
		for (; it; it = it->next) {
			if (implicit && !it->implicit) {
				continue;
			}

			// TODO check traits

			CastingOption* newTarget = ketlGetFreeObjectFromPool(&irBuilder->castingPool);
			newTarget->variable = variable;
			newTarget->operator = it;

			if (targets.last == NULL) {
				targets.last = newTarget;
			}
			newTarget->next = targets.begin;
			targets.begin = newTarget;

			newTarget->score = !numberLiteral || it->outputType->size != MAX_SIZE_OF_NUMBER_LITERAL;
		}
	}

	CastingOption* newTarget = ketlGetFreeObjectFromPool(&irBuilder->castingPool);
	newTarget->variable = variable;
	newTarget->operator = NULL;

	if (targets.last == NULL) {
		targets.last = newTarget;
	}
	newTarget->next = targets.begin;
	targets.begin = newTarget;

	newTarget->score = numberLiteral && variable->type->size != MAX_SIZE_OF_NUMBER_LITERAL;

	return targets;
}

static CastingOption* possibleCastingForDelegate(KETLIRBuilder* irBuilder, IRUndefinedDelegate* udelegate) {
	CastingOption* targets = NULL;

	IRUndefinedValue* callerIt = udelegate->caller;
	for (; callerIt; callerIt = callerIt->next) {
		KETLIRVariable* callerValue = callerIt->variable;
		KETLType* callerType = callerValue->type;


		KETLIRVariable* outputValue = callerValue;

		if (callerType->kind == KETL_TYPE_KIND_FUNCTION) {
			__debugbreak();
		}
		else {
			if (udelegate->arguments) {
				// only functions can use arguments
				continue;
			}
		}

		TypeCastingTargetList castingTargets = possibleCastingForValue(irBuilder, outputValue, true);
		if (castingTargets.begin != NULL) {
			castingTargets.last->next = targets;
			targets = castingTargets.begin;
		}
	}

	return targets;
}

KETL_DEFINE(BinaryOperatorDeduction) {
	KETLBinaryOperator* operator;
	CastingOption* lhsCasting;
	CastingOption* rhsCasting;
};

static BinaryOperatorDeduction deduceInstructionCode2(KETLIRBuilder* irBuilder, KETLSyntaxNodeType syntaxOperator, IRUndefinedDelegate* lhs, IRUndefinedDelegate* rhs) {
	BinaryOperatorDeduction result;
	result.operator = NULL;
	result.lhsCasting = NULL;
	result.rhsCasting = NULL;

	KETLOperatorCode operatorCode = syntaxOperator - KETL_SYNTAX_NODE_TYPE_OPERATOR_OFFSET;
	KETLState* state = irBuilder->state;

	KETLBinaryOperator** pOperatorResult = ketlIntMapGet(&state->binaryOperators, operatorCode);
	if (pOperatorResult == NULL) {
		return result;
	}
	KETLBinaryOperator* operatorResult = *pOperatorResult;

	uint64_t bestScore = UINT64_MAX;

	uint64_t currentScore;

	CastingOption* lhsCasting = possibleCastingForDelegate(irBuilder, lhs);
	CastingOption* rhsCasting = possibleCastingForDelegate(irBuilder, rhs);

	KETLBinaryOperator* it = operatorResult;
	for (; it; it = it->next) {
		currentScore = 0;
		CastingOption* lhsIt = lhsCasting;
		for (; lhsIt; lhsIt = lhsIt->next) {
			KETLType* lhsType = lhsIt->operator ? lhsIt->operator->outputType : lhsIt->variable->type;
			if (it->lhsType != lhsType) {
				continue;
			}

			CastingOption* rhsIt = rhsCasting;
			for (; rhsIt; rhsIt = rhsIt->next) {
				KETLType* rhsType = rhsIt->operator ? rhsIt->operator->outputType : rhsIt->variable->type;
				if (it->rhsType != rhsType) {
					continue;
				}

				currentScore += lhsIt->score;
				currentScore += rhsIt->score;

				if (currentScore < bestScore) {
					bestScore = currentScore;
					result.operator = it;
					result.lhsCasting = lhsIt;
					result.rhsCasting = rhsIt;
				}
				else if (currentScore == bestScore) {
					result.operator = NULL;
				}
			}
		}
	}

	return result;
}

static CastingOption* castDelegateToVariable(KETLIRBuilder* irBuilder, IRUndefinedDelegate* udelegate, KETLType* type) {
	CastingOption* options = possibleCastingForDelegate(irBuilder, udelegate);
	CastingOption* it = options;

	for (; it; it = it->next) {
		KETLType* castingType = it->operator ? it->operator->outputType : it->variable->type;
		if (castingType == type) {
			return it;
		}
	}

	return NULL;
}

static CastingOption* getBestCastingOptionForDelegate(KETLIRBuilder* irBuilder, IRUndefinedDelegate* udelegate) {
	CastingOption* options = possibleCastingForDelegate(irBuilder, udelegate);
	CastingOption* it = options;
	CastingOption* best = NULL;
	uint64_t bestScore = UINT64_MAX;

	for (; it; it = it->next) {
		if (it->score == 0) {
			// TODO temporary fix
			return it;
		}
		if (it->score < bestScore) {
			bestScore = it->score;
			best = it;
		}
		else if (it->score == bestScore) {
			best = NULL;
		}
	}

	return best;
}

static IRUndefinedDelegate* buildIRCommandTree(KETLIRFunctionWIP* wip, IROperationRange* operationRange, KETLSyntaxNode* syntaxNodeRoot) {
	KETLSyntaxNode* it = syntaxNodeRoot;
	switch (it->type) {
	case KETL_SYNTAX_NODE_TYPE_ID: {
		const char* uniqName = ketlAtomicStringsGet(&wip->builder->state->strings, it->value, it->length);
		IRUndefinedValue** ppValue = ketlIntMapGet(&wip->builder->variablesMap, (KETLIntMapKey)uniqName);
		if (ppValue == NULL) {
			ppValue = ketlIntMapGet(&wip->builder->state->globalNamespace.variables, (KETLIntMapKey)uniqName);
		}
		if (ppValue == NULL) {
			// TODO error
			__debugbreak();
		}

		return wrapInDelegateUValue(wip->builder, *ppValue);
	}
	case KETL_SYNTAX_NODE_TYPE_NUMBER: {
		Literal literal = parseLiteral(wip, it->value, it->length);
		if (literal.type == NULL) {
			// TODO error
			__debugbreak();
		}
		KETLIRVariable* variable = ketlGetFreeObjectFromPool(&wip->builder->variablesPool);
		variable->type = literal.type;
		variable->traits.isNullable = false;
		variable->traits.isConst = true;
		variable->traits.type = KETL_TRAIT_TYPE_LITERAL;
		variable->value = literal.value;

		return wrapInDelegateValue(wip->builder, variable);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PROD:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_DIV:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_EQUAL:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_UNEQUAL: {
		IROperationRange innerRange;
		initInnerOperationRange(wip->builder, operationRange, &innerRange);
		KETLIRScopedVariable* resultVariable = createTempVariable(wip);

		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRCommandTree(wip, &innerRange, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRCommandTree(wip, &innerRange, rhsNode);

		ketlResetPool(&wip->builder->castingPool);
		BinaryOperatorDeduction deductionStruct = deduceInstructionCode2(wip->builder, it->type, lhs, rhs);
		if (deductionStruct.operator == NULL) {
			// TODO error
			__debugbreak();
		}

		convertValues(wip, &innerRange, deductionStruct.lhsCasting);
		convertValues(wip, &innerRange, deductionStruct.rhsCasting);

		deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

		KETLBinaryOperator deduction = *deductionStruct.operator;
		KETLIROperation* operation = createOperationFromRange(wip->builder, operationRange);
		operation->code = deduction.code;
		operation->argumentCount = 3;
		operation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 3);
		operation->arguments[1] = &deductionStruct.lhsCasting->variable->value;
		operation->arguments[2] = &deductionStruct.rhsCasting->variable->value;

		operation->arguments[0] = &resultVariable->variable.value;
		resultVariable->variable.type = deduction.outputType;
		resultVariable->variable.traits = deduction.outputTraits;

		operation->extraNext = NULL;

		return wrapInDelegateValue(wip->builder, &resultVariable->variable);;
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN: {
		IROperationRange innerRange;
		initInnerOperationRange(wip->builder, operationRange, &innerRange);

		KETLSyntaxNode* lhsNode = it->firstChild;
		IRUndefinedDelegate* lhs = buildIRCommandTree(wip, &innerRange, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRUndefinedDelegate* rhs = buildIRCommandTree(wip, &innerRange, rhsNode);

		deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

		KETLIROperation* operation = createOperationFromRange(wip->builder, operationRange);

		operation->code = KETL_IR_CODE_ASSIGN_8_BYTES; // TODO choose from type
		operation->argumentCount = 2;
		operation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 2);
		operation->arguments[0] = &lhs->caller->variable->value; // TODO actual convertion from udelegate to correct type
		operation->arguments[1] = &rhs->caller->variable->value; // TODO actual convertion from udelegate to correct type

		operation->extraNext = NULL;

		return wrapInDelegateValue(wip->builder, lhs->caller->variable);
	}
	KETL_NODEFAULT();
	__debugbreak();
	}
	return NULL;
}

#define UPDATE_NODE_TALE(firstVariable)\
do {\
	while (firstVariable->parent) {\
	firstVariable = firstVariable->parent;\
	}\
	\
	if (currentStack != NULL) {\
		KETLIRScopedVariable* lastChild = currentStack->firstChild;\
		if (lastChild != NULL) {\
			while (lastChild->nextSibling) {\
				lastChild = lastChild->nextSibling;\
			}\
			lastChild->nextSibling = firstVariable;\
		}\
		else {\
			currentStack->firstChild = firstVariable;\
		}\
		firstVariable->parent = currentStack;\
	}\
	else {\
		if (stackRoot != NULL) {\
			KETLIRScopedVariable* lastChild = stackRoot;\
			while (lastChild->nextSibling) {\
				lastChild = lastChild->nextSibling;\
			}\
			lastChild->nextSibling = firstVariable;\
		}\
		else {\
			stackRoot = firstVariable;\
		}\
		firstVariable->parent = NULL;\
	}\
} while(0)\

static inline void restoreLocalSopeContext(KETLIRFunctionWIP* wip, KETLIRScopedVariable* currentStack, KETLIRScopedVariable* stackRoot) {
	{
		// set local variable
		KETLIRScopedVariable* firstLocalVariable = wip->localVariables;
		if (firstLocalVariable != NULL) {
			KETLIRScopedVariable* lastLocalVariables = firstLocalVariable;
			UPDATE_NODE_TALE(firstLocalVariable);
			currentStack = firstLocalVariable;
		}
	}

	{
		// set temp variable
		KETLIRScopedVariable* firstTempVariable = wip->tempVariables;
		if (firstTempVariable) {
			UPDATE_NODE_TALE(firstTempVariable);
		}
	}

	wip->currentStack = currentStack;
	wip->stackRoot = stackRoot;
}

static inline void restoreScopeContext(KETLIRFunctionWIP* wip, KETLIRScopedVariable* savedStack, uint64_t scopeIndex) {
	wip->scopeIndex = scopeIndex;
	wip->currentStack = savedStack;

	KETLIntMapIterator iterator;
	ketlInitIntMapIterator(&iterator, &wip->builder->variablesMap);

	while (ketlIntMapIteratorHasNext(&iterator)) {
		const char* name;
		IRUndefinedValue** pCurrent;
		ketlIntMapIteratorGet(&iterator, (KETLIntMapKey*)&name, &pCurrent);
		IRUndefinedValue* current = *pCurrent;
		KETL_FOREVER{
			if (current == NULL) {
				ketlIntMapIteratorRemove(&iterator);
				break;
			}

			if (current->scopeIndex <= scopeIndex) {
				*pCurrent = current;
				ketlIntMapIteratorNext(&iterator);
				break;
			}

			current = current->next;
		}
	}
}

static void createVariableDefinition(KETLIRFunctionWIP* wip, IROperationRange* operationRange, KETLSyntaxNode* idNode, KETLType* type) {
	if (KETL_CHECK_VOE(idNode->type == KETL_SYNTAX_NODE_TYPE_ID)) {
		return;
	}

	IROperationRange innerRange;
	initInnerOperationRange(wip->builder, operationRange, &innerRange);

	KETLIRScopedVariable* variable = createLocalVariable(wip);

	KETLSyntaxNode* expressionNode = idNode->nextSibling;
	IRUndefinedDelegate* expression = buildIRCommandTree(wip, &innerRange, expressionNode);

	const char* name = ketlAtomicStringsGet(&wip->builder->state->strings, idNode->value, idNode->length);

	uint64_t scopeIndex = wip->scopeIndex;

	IRUndefinedValue** pCurrent;
	if (ketlIntMapGetOrCreate(&wip->builder->variablesMap, (KETLIntMapKey)name, &pCurrent)) {
		*pCurrent = NULL;
	}
	else {
		// TODO check if the type is function, then we can overload
		if ((*pCurrent)->scopeIndex == scopeIndex) {
			// TODO error
			__debugbreak();
		}
	}
	IRUndefinedValue* uvalue = wrapInUValueVariable(wip->builder, &variable->variable);
	uvalue->scopeIndex = scopeIndex;
	uvalue->next = *pCurrent;
	*pCurrent = uvalue;

	if (expression == NULL) {
		// TODO error
		__debugbreak();
	}

	// TODO set traits properly from syntax node
	variable->variable.traits.isConst = false;
	variable->variable.traits.isNullable = false;
	variable->variable.traits.type = KETL_TRAIT_TYPE_LVALUE;

	CastingOption* expressionCasting;

	ketlResetPool(&wip->builder->castingPool);
	if (type == NULL) {
		expressionCasting = getBestCastingOptionForDelegate(wip->builder, expression);
	}
	else {
		expressionCasting = castDelegateToVariable(wip->builder, expression, type);
	}

	if (expressionCasting == NULL) {
		// TODO error
		__debugbreak();
	}

	convertValues(wip, &innerRange, expressionCasting);

	deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

	KETLIRVariable* expressionVarible = expressionCasting->variable;
	variable->variable.type = expressionVarible->type;

	KETLIROperation* operation = createOperationFromRange(wip->builder, operationRange);

	operation->code = KETL_IR_CODE_ASSIGN_8_BYTES; // TODO choose from type
	operation->argumentCount = 2;
	operation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 2);
	operation->arguments[0] = &variable->variable.value;
	operation->arguments[1] = &expressionVarible->value;

	operation->extraNext = NULL;
}

static void buildIRCommandLoopIteration(KETLIRFunctionWIP* wip, IROperationRange* operationRange, KETLSyntaxNode* syntaxNode);

static void buildIRCommand(KETLIRFunctionWIP* wip, IROperationRange* operationRange, KETLSyntaxNode* syntaxNode) {
	bool isNull = syntaxNode == NULL;
	if (isNull) {
		return;
	}
	bool nonLast = syntaxNode->nextSibling != NULL;
	if (!nonLast) {
		buildIRCommandLoopIteration(wip, operationRange, syntaxNode);
		return;
	}

	IROperationRange innerRange;
	initInnerOperationRange(wip->builder, operationRange, &innerRange);

	do {
		buildIRCommandLoopIteration(wip, &innerRange, syntaxNode);
		syntaxNode = syntaxNode->nextSibling;
		nonLast = syntaxNode->nextSibling != NULL;
	} while (nonLast);

	deinitInnerOperationRange(wip->builder, operationRange, &innerRange);
	buildIRCommandLoopIteration(wip, operationRange, syntaxNode);
}

static void buildIRCommandLoopIteration(KETLIRFunctionWIP* wip, IROperationRange* operationRange, KETLSyntaxNode* syntaxNode) {
	switch (syntaxNode->type) {
	case KETL_SYNTAX_NODE_TYPE_BLOCK: {
		KETLIRScopedVariable* savedStack = wip->currentStack;
		uint64_t scopeIndex = wip->scopeIndex++;

		buildIRCommand(wip, operationRange, syntaxNode->firstChild);

		restoreScopeContext(wip, savedStack, scopeIndex);
		break;
	}
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR: {
		KETLIRScopedVariable* currentStack = wip->currentStack;
		KETLIRScopedVariable* stackRoot = wip->stackRoot;

		wip->tempVariables = NULL;
		wip->localVariables = NULL;

		KETLSyntaxNode* idNode = syntaxNode->firstChild;

		createVariableDefinition(wip, operationRange, idNode, NULL);

		restoreLocalSopeContext(wip, currentStack, stackRoot);
		break;
	}
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE: {
		KETLIRScopedVariable* currentStack = wip->currentStack;
		KETLIRScopedVariable* stackRoot = wip->stackRoot;

		wip->tempVariables = NULL;
		wip->localVariables = NULL;

		KETLSyntaxNode* typeNode = syntaxNode->firstChild;

		KETLState* state = wip->builder->state;
		const char* typeName = ketlAtomicStringsGet(&state->strings, typeNode->value, typeNode->length);

		KETLType* type = ketlIntMapGet(&state->globalNamespace.types, (KETLIntMapKey)typeName);

		KETLSyntaxNode* idNode = typeNode->nextSibling;

		createVariableDefinition(wip, operationRange, idNode, type);

		restoreLocalSopeContext(wip, currentStack, stackRoot);
		break;
	}
	case KETL_SYNTAX_NODE_TYPE_IF_ELSE: {
		KETLIRScopedVariable* savedStack = wip->currentStack;
		uint64_t scopeIndex = wip->scopeIndex++;

		KETLIRScopedVariable* currentStack = wip->currentStack;
		KETLIRScopedVariable* stackRoot = wip->stackRoot;

		wip->tempVariables = NULL;
		wip->localVariables = NULL;

		IROperationRange innerRange;
		initInnerOperationRange(wip->builder, operationRange, &innerRange);

		KETLSyntaxNode* expressionNode = syntaxNode->firstChild;
		IRUndefinedDelegate* expression = buildIRCommandTree(wip, &innerRange, expressionNode);

		CastingOption* expressionCasting = castDelegateToVariable(wip->builder, expression, wip->builder->state->primitives.bool_t);
		if (expressionCasting == NULL) {
			// TODO error
			__debugbreak();
		}
		convertValues(wip, &innerRange, expressionCasting);

		restoreLocalSopeContext(wip, currentStack, stackRoot);

		KETLIROperation* ifJumpOperation = createOperationFromRange(wip->builder, &innerRange);
		ifJumpOperation->code = KETL_IR_CODE_JUMP_IF_FALSE;
		ifJumpOperation->argumentCount = 1;
		ifJumpOperation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 1);
		ifJumpOperation->arguments[0] = &expressionCasting->variable->value;

		deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

		// builds true instructions
		KETLSyntaxNode* trueBlockNode = expressionNode->nextSibling;
		KETLSyntaxNode* falseBlockNode = trueBlockNode->nextSibling;

		if (falseBlockNode == NULL) {
			ifJumpOperation->extraNext = operationRange->next;

			// builds true instructions
			KETLSyntaxNode* trueBlockNode = expressionNode->nextSibling;
			KETLIROperation* checkTrueRoot = operationRange->root;
			buildIRCommandLoopIteration(wip, operationRange, trueBlockNode);
			KETLIROperation* newRoot = operationRange->root;
			if (checkTrueRoot == newRoot) {
				operationRange->root = operationRange->next;
				operationRange->next = newRoot;

				ifJumpOperation->mainNext = operationRange->root;
			}
			else if (NULL == newRoot) {
				operationRange->root = operationRange->next;
				operationRange->next = createOperationImpl(wip->builder);
			}
		}
		else {
			IROperationRange sideRange;
			initSideOperationRange(wip->builder, operationRange, &sideRange);

			ifJumpOperation->extraNext = sideRange.root;

			// builds true instructions
			KETLIROperation* checkTrueRoot = operationRange->root;
			buildIRCommandLoopIteration(wip, operationRange, trueBlockNode);
			KETLIROperation* newRoot = operationRange->root;
			if (checkTrueRoot == newRoot) {
				operationRange->root = operationRange->next;
				operationRange->next = newRoot;

				ifJumpOperation->mainNext = operationRange->root;

				// builds false instructions
				buildIRCommandLoopIteration(wip, &sideRange, falseBlockNode);
				// TODO check if false block is empty
				deinitSideOperationRange(wip->builder, &sideRange);
			}
			else if (NULL != newRoot) {
				// builds false instructions
				buildIRCommandLoopIteration(wip, &sideRange, falseBlockNode);
				// TODO check if false block is empty
				deinitSideOperationRange(wip->builder, &sideRange);
			}
			else {
				// builds false instructions
				buildIRCommandLoopIteration(wip, &sideRange, falseBlockNode);
				*operationRange = sideRange;
			}
		}

		restoreScopeContext(wip, savedStack, scopeIndex);
		break;
	}
	case KETL_SYNTAX_NODE_TYPE_RETURN: {
		KETLSyntaxNode* expressionNode = syntaxNode->firstChild;
		if (expressionNode) {
			KETLIRScopedVariable* currentStack = wip->currentStack;
			KETLIRScopedVariable* stackRoot = wip->stackRoot;

			wip->tempVariables = NULL;
			wip->localVariables = NULL;

			IROperationRange innerRange;
			initInnerOperationRange(wip->builder, operationRange, &innerRange);
			IRUndefinedDelegate* expression = buildIRCommandTree(wip, &innerRange, expressionNode);
			deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

			KETLIROperation* assignOperation = createLastOperationFromRange(wip->builder, operationRange);
			assignOperation->code = KETL_IR_CODE_RETURN_8_BYTES; // TODO deside from type
			assignOperation->argumentCount = 1;
			assignOperation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 1);
			assignOperation->arguments[0] = &expression->caller->variable->value; // TODO actual convertion from udelegate to correct type

			operationRange->root = innerRange.next;

			restoreLocalSopeContext(wip, currentStack, stackRoot);
		}
		else {
			KETLIROperation* operation = createLastOperationFromRange(wip->builder, operationRange);
			operation->code = KETL_IR_CODE_RETURN;
			operation->argumentCount = 0;
		}

		break;
	}
	default: {
		KETLIRScopedVariable* currentStack = wip->currentStack;
		KETLIRScopedVariable* stackRoot = wip->stackRoot;

		wip->tempVariables = NULL;
		wip->localVariables = NULL;

		IRUndefinedDelegate* result = buildIRCommandTree(wip, operationRange, syntaxNode);

		restoreLocalSopeContext(wip, currentStack, stackRoot);
	}
	}
}

static KETLIROperation* collectReturnOperations(KETLIROperation* rootOperation, KETLIRBuilder* irBuilder) {
	KETL_FOREVER {
		KETLIROperation* extraNext = rootOperation->extraNext;
		if (extraNext != NULL) {
			*(KETLIROperation**)ketlPushOnStack(&irBuilder->extraNextStack) = extraNext;
		}

		KETLIROperation* nextOperation = rootOperation->mainNext;
		if (nextOperation == NULL) {
			return rootOperation;
		}
		rootOperation = nextOperation;
	}
}

static void countOperationsAndArguments(KETLIROperation* rootOperation, KETLIRBuilder* irBuilder) {
	while(rootOperation != NULL) {
		uint64_t* newRefer;
		if (!ketlIntMapGetOrCreate(&irBuilder->operationReferMap, (KETLIntMapKey)rootOperation, &newRefer)) {
			return;
		}
		*newRefer = ketlIntMapGetSize(&irBuilder->operationReferMap) - 1;

		uint64_t* newIndex;
		for (uint64_t i = 0; i < rootOperation->argumentCount; ++i) {
			if (ketlIntMapGetOrCreate(&irBuilder->argumentsMap, (KETLIntMapKey)rootOperation->arguments[i], &newIndex)) {
				*newIndex = ketlIntMapGetSize(&irBuilder->argumentsMap) - 1;
			}
		}
		
		KETLIROperation* extraNext = rootOperation->extraNext;
		if (extraNext != NULL) {
			*(KETLIROperation**)ketlPushOnStack(&irBuilder->extraNextStack) = extraNext;
		}
		rootOperation = rootOperation->mainNext;
	}
}

static inline uint64_t bakeStackUsage(KETLIRScopedVariable* stackRoot) {
	// TODO align
	uint64_t currentStackOffset = 0;
	uint64_t maxStackOffset = 0;

	KETLIRScopedVariable* it = stackRoot;
	if (it == NULL) {
		return maxStackOffset;
	}

	KETL_FOREVER{
		it->variable.value.stack = currentStackOffset;

		uint64_t size = getStackTypeSize(it->variable.traits, it->variable.type);
		currentStackOffset += size;
		if (maxStackOffset < currentStackOffset) {
			maxStackOffset = currentStackOffset;
		}

		if (it->firstChild) {
			it = it->firstChild;
		}
		else {
			currentStackOffset -= size;
			while (it->nextSibling == NULL) {
				if (it->parent == NULL) {
					return maxStackOffset;
				}
				it = it->parent;
				size = getStackTypeSize(it->variable.traits, it->variable.type);
				currentStackOffset -= size;
			}
			it = it->nextSibling;
		}
	}
}

KETL_DEFINE(ReturnNode) {
	KETLIROperation* operation;
	ReturnNode* next;
};

KETLIRFunction* ketlBuildIR(KETLType* returnType, KETLIRBuilder* irBuilder, KETLSyntaxNode* syntaxNodeRoot, KETLType* argumentType, const char* argumentName) {
	KETLIRFunctionWIP wip;

	wip.builder = irBuilder;

	ketlIntMapReset(&irBuilder->variablesMap);

	ketlResetPool(&irBuilder->scopedVariablesPool);
	ketlResetPool(&irBuilder->variablesPool);
	ketlResetPool(&irBuilder->operationsPool);
	ketlResetPool(&irBuilder->udelegatePool);
	ketlResetPool(&irBuilder->uvaluePool);
	ketlResetPool(&irBuilder->argumentPointersPool);
	ketlResetPool(&irBuilder->argumentsPool);

	ketlIntMapReset(&irBuilder->operationReferMap);
	ketlIntMapReset(&irBuilder->argumentsMap);

	wip.stackRoot = NULL;
	wip.currentStack = NULL;

	wip.scopeIndex = 1;

	if (argumentType && argumentName) {
		KETLIRVariable* argument = ketlGetFreeObjectFromPool(&irBuilder->variablesPool);
		argument->value.type = KETL_IR_ARGUMENT_TYPE_ARGUMENT;
		argument->value.stack = 0;

		argument->traits.isConst = false;
		argument->traits.isNullable = false;
		argument->traits.type = KETL_TRAIT_TYPE_LVALUE;
		argument->type = argumentType;

		const char* name = ketlAtomicStringsGet(&irBuilder->state->strings, argumentName, KETL_NULL_TERMINATED_LENGTH);

		uint64_t scopeIndex = 0;

		IRUndefinedValue** pCurrent;
		if (ketlIntMapGetOrCreate(&irBuilder->variablesMap, (KETLIntMapKey)name, &pCurrent)) {
			*pCurrent = NULL;
		}
		else {
			// TODO check if the type is function, then we can overload
			if ((*pCurrent)->scopeIndex == scopeIndex) {
				// TODO error
				__debugbreak();
			}
		}
		IRUndefinedValue* uvalue = wrapInUValueVariable(irBuilder, argument);
		uvalue->scopeIndex = scopeIndex;
		uvalue->next = *pCurrent;
		*pCurrent = uvalue;
	}

	KETLIROperation* rootOperation = createOperationImpl(irBuilder);
	IROperationRange range;
	range.root = rootOperation;
	range.next = createOperationImpl(irBuilder);
	buildIRCommand(&wip, &range, syntaxNodeRoot);

	if (range.root != NULL) {
		KETLIROperation* operation = createLastOperationFromRange(irBuilder, &range);
		operation->code = KETL_IR_CODE_RETURN;
		operation->argumentCount = 0;
	}
	// range.next would be not used, 
	// but we don't need to recycle it, 
	// since the building cycle came to the end

	if (returnType == NULL) {
		// TODO auto determine returnType

		KETLStack returnStack;
		ketlInitStack(&returnStack, sizeof(KETLIROperation*), 16);

		ketlResetStack(&irBuilder->extraNextStack);

		*(KETLIROperation**)ketlPushOnStack(&irBuilder->extraNextStack) = rootOperation;

		while (!ketlIsStackEmpty(&irBuilder->extraNextStack)) {
			KETLIROperation* itOperation = *(KETLIROperation**)ketlPeekStack(&irBuilder->extraNextStack);
			ketlPopStack(&irBuilder->extraNextStack);

			*(KETLIROperation**)ketlPushOnStack(&returnStack) = collectReturnOperations(itOperation, irBuilder);
		}

		KETLStackIterator returnIterator;
		ketlInitStackIterator(&returnIterator, &returnStack);

		bool isProperReturnVoid = true;
		bool isProperReturnValue = true;

		while (!ketlIsStackEmpty(&returnStack)) {
			KETLIROperation* operation = *(KETLIROperation**)ketlPeekStack(&returnStack);

			isProperReturnVoid &= operation->code == KETL_IR_CODE_RETURN;
			isProperReturnValue &= operation->code != KETL_IR_CODE_RETURN;

			ketlPopStack(&returnStack);
		}

		if (isProperReturnVoid && !isProperReturnValue) {
			// TODO set as void
		}
		else {
			// TODO check the common conversion and convert
		}

		ketlDeinitStack(&returnStack);
	}
	else {

		KETLStack returnStack;
		ketlInitStack(&returnStack, sizeof(KETLIROperation*), 16);

		ketlResetStack(&irBuilder->extraNextStack);

		*(KETLIROperation**)ketlPushOnStack(&irBuilder->extraNextStack) = rootOperation;

		while (!ketlIsStackEmpty(&irBuilder->extraNextStack)) {
			KETLIROperation* itOperation = *(KETLIROperation**)ketlPeekStack(&irBuilder->extraNextStack);
			ketlPopStack(&irBuilder->extraNextStack);

			*(KETLIROperation**)ketlPushOnStack(&returnStack) = collectReturnOperations(itOperation, irBuilder);
		}

		if (returnType == irBuilder->state->primitives.void_t) {
			while (!ketlIsStackEmpty(&returnStack)) {
				KETLIROperation* operation = *(KETLIROperation**)ketlPeekStack(&returnStack);

				if (operation->code != KETL_IR_CODE_RETURN) {
					// TODO error
					return NULL;
				}

				ketlPopStack(&returnStack);
			}
		}
		else {
			while (!ketlIsStackEmpty(&returnStack)) {
				KETLIROperation* operation = *(KETLIROperation**)ketlPeekStack(&returnStack);

				// TODO properly check that all exit point has correct return
				if (operation->code == KETL_IR_CODE_RETURN) {
					// TODO error
					return NULL;
				}

				ketlPopStack(&returnStack);
			}
		}

		ketlDeinitStack(&returnStack);
	}

	ketlIntMapReset(&irBuilder->operationReferMap);
	ketlIntMapReset(&irBuilder->argumentsMap);
	ketlResetStack(&irBuilder->extraNextStack);

	*(KETLIROperation**)ketlPushOnStack(&irBuilder->extraNextStack) = rootOperation;

	while (!ketlIsStackEmpty(&irBuilder->extraNextStack)) {
		KETLIROperation* itOperation = *(KETLIROperation**)ketlPeekStack(&irBuilder->extraNextStack);
		ketlPopStack(&irBuilder->extraNextStack);
		countOperationsAndArguments(itOperation, irBuilder);
	}

	uint64_t operationCount = ketlIntMapGetSize(&irBuilder->operationReferMap);
	uint64_t argumentsCount = ketlIntMapGetSize(&irBuilder->argumentsMap);

	uint64_t maxStackOffset = bakeStackUsage(wip.stackRoot);

	KETLIRFunctionDefinition functionDefinition;

	// TODO align
	uint64_t totalSize = sizeof(KETLIRFunction) + sizeof(KETLIRArgument) * argumentsCount + sizeof(KETLIROperation) * operationCount;
	uint8_t* rawPointer = malloc(totalSize);

 	functionDefinition.function = (KETLIRFunction*)rawPointer;
	rawPointer += sizeof(KETLIRFunction);

	functionDefinition.function->stackUsage = maxStackOffset;

	functionDefinition.function->arguments = (KETLIRArgument*)rawPointer;
	rawPointer += sizeof(KETLIRArgument) * argumentsCount;

	KETLIntMapIterator argumentIterator;
	ketlInitIntMapIterator(&argumentIterator, &irBuilder->argumentsMap);
	while (ketlIntMapIteratorHasNext(&argumentIterator)) {
		KETLIRArgument* oldArgument;
		uint64_t* argumentNewIndex;
		ketlIntMapIteratorGet(&argumentIterator, (KETLIntMapKey*)&oldArgument, &argumentNewIndex);
		functionDefinition.function->arguments[*argumentNewIndex] = *oldArgument;
		ketlIntMapIteratorNext(&argumentIterator);
	}

	functionDefinition.function->operationsCount = operationCount;
	functionDefinition.function->operations = (KETLIROperation*)rawPointer;

	KETLIntMapIterator operationIterator;
	ketlInitIntMapIterator(&operationIterator, &irBuilder->operationReferMap);
	while (ketlIntMapIteratorHasNext(&operationIterator)) {
		KETLIROperation* pOldOperation;
		uint64_t* operationNewIndex;
		ketlIntMapIteratorGet(&operationIterator, (KETLIntMapKey*)&pOldOperation, &operationNewIndex);
		KETLIROperation oldOperation = *pOldOperation;
		KETLIRArgument* arguments = functionDefinition.function->arguments;
		for (uint64_t i = 0; i < oldOperation.argumentCount; ++i) {
			uint64_t* argumentNewIndex = ketlIntMapGet(&irBuilder->argumentsMap, (KETLIntMapKey)oldOperation.arguments[i]);
			oldOperation.arguments[i] = &arguments[*argumentNewIndex];
		}
		if (oldOperation.mainNext != NULL) {
			uint64_t* operationNewNextIndex = ketlIntMapGet(&irBuilder->operationReferMap, (KETLIntMapKey)oldOperation.mainNext);
			oldOperation.mainNext = &functionDefinition.function->operations[*operationNewNextIndex];
		}
		if (oldOperation.extraNext != NULL) {
			uint64_t* operationNewNextIndex = ketlIntMapGet(&irBuilder->operationReferMap, (KETLIntMapKey)oldOperation.extraNext);
			oldOperation.extraNext = &functionDefinition.function->operations[*operationNewNextIndex];
		}
		functionDefinition.function->operations[*operationNewIndex] = oldOperation;
		ketlIntMapIteratorNext(&operationIterator);
	}

	functionDefinition.type = NULL;


	return functionDefinition.function;
}