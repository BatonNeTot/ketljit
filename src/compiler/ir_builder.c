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

	ketlInitIntMap(&irBuilder->variablesMap, sizeof(IRUndefinedValue*), 16);
	ketlInitObjectPool(&irBuilder->scopedVariablesPool, sizeof(KETLIRScopedVariable), 16);
	ketlInitObjectPool(&irBuilder->variablesPool, sizeof(KETLIRVariable), 16);
	ketlInitObjectPool(&irBuilder->operationsPool, sizeof(KETLIROperation), 16);
	ketlInitObjectPool(&irBuilder->udelegatePool, sizeof(IRUndefinedDelegate), 16);
	ketlInitObjectPool(&irBuilder->uvaluePool, sizeof(IRUndefinedValue), 16);
	ketlInitObjectPool(&irBuilder->argumentPointersPool, sizeof(KETLIRArgument*), 32);
	ketlInitObjectPool(&irBuilder->argumentsPool, sizeof(KETLIRArgument), 16);

	ketlInitIntMap(&irBuilder->operationReferMap, sizeof(uint64_t), 16);
	ketlInitIntMap(&irBuilder->argumentsMap, sizeof(uint64_t), 16);

	ketlInitObjectPool(&irBuilder->castingPool, sizeof(CastingOption), 16);
}

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder) {
	ketlDeinitObjectPool(&irBuilder->castingPool);

	ketlDeinitIntMap(&irBuilder->argumentsMap);
	ketlDeinitIntMap(&irBuilder->operationReferMap);

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

static inline KETLIROperation* createOperation(KETLIRBuilder* irBuilder) {
	return ketlGetFreeObjectFromPool(&irBuilder->operationsPool);
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

KETL_DEFINE(IRConvertionResult) {
	KETLIRVariable* variable;
	KETLIROperation* nextOperation;
};

static IRConvertionResult convertValues(KETLIRFunctionWIP* wip, KETLIROperation* rootOperation, CastingOption* castingOption) {
	IRConvertionResult convertionResult;
	convertionResult.variable = castingOption->variable;
	convertionResult.nextOperation = rootOperation;

	if (castingOption->variable->traits.type == KETL_TRAIT_TYPE_LITERAL) {
		convertLiteralSize(castingOption->variable,
			castingOption->operator ? castingOption->operator->outputType : castingOption->variable->type);
	}
	else {
		if (castingOption == NULL || castingOption->operator == NULL) {
			return convertionResult;
		}

		KETLIRVariable* result = &createTempVariable(wip)->variable;

		KETLCastOperator casting = *castingOption->operator;
		rootOperation->code = casting.code;
		rootOperation->argumentCount = 2;
		rootOperation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 2);
		rootOperation->arguments[1] = &castingOption->variable->value;

		rootOperation->arguments[0] = &result->value;
		result->type = casting.outputType;
		result->traits = casting.outputTraits;

		castingOption->variable = result;
		convertionResult.variable = result;
		convertionResult.nextOperation = createOperation(wip->builder);
	}

	return convertionResult;
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

KETL_DEFINE(IRResult) {
	IRUndefinedDelegate* delegate;
	KETLIROperation* nextOperation;
};

static  IRResult buildIRFromSyntaxNode(KETLIRFunctionWIP* wip, KETLIROperation* rootOperation, KETLSyntaxNode* syntaxNodeRoot);
static KETLIROperation* buildIRBlock(KETLIRFunctionWIP* wip, KETLIROperation* rootOperation, KETLSyntaxNode* syntaxNode);

static IRConvertionResult createVariableDefinition(KETLIRFunctionWIP* wip, KETLIROperation* rootOperation, KETLSyntaxNode* idNode, KETLType* type) {
	IRConvertionResult variableDefinitionResult;
	variableDefinitionResult.variable = NULL;
	variableDefinitionResult.nextOperation = rootOperation;

	if (KETL_CHECK_VOE(idNode->type == KETL_SYNTAX_NODE_TYPE_ID)) {
		return variableDefinitionResult;
	}

	KETLIRScopedVariable* variable = createLocalVariable(wip);

	KETLSyntaxNode* expressionNode = idNode->nextSibling;
	IRResult expression = buildIRFromSyntaxNode(wip, rootOperation, expressionNode);

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

	if (expression.delegate == NULL) {
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
		expressionCasting = getBestCastingOptionForDelegate(wip->builder, expression.delegate);
	}
	else {
		expressionCasting = castDelegateToVariable(wip->builder, expression.delegate, type);
	}

	if (expressionCasting == NULL) {
		// TODO error
		__debugbreak();
	}

	IRConvertionResult conversionResult = convertValues(wip, expression.nextOperation, expressionCasting);

	KETLIRVariable* expressionVarible = expressionCasting->variable;
	variable->variable.type = expressionVarible->type;

	KETLIROperation* operation = conversionResult.nextOperation;

	operation->code = KETL_IR_CODE_ASSIGN_8_BYTES; // TODO choose from type
	operation->argumentCount = 2;
	operation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 2);
	operation->arguments[0] = &variable->variable.value;
	operation->arguments[1] = &expressionVarible->value;

	variableDefinitionResult.variable = &variable->variable;
	variableDefinitionResult.nextOperation = createOperation(wip->builder);

	operation->mainNext = variableDefinitionResult.nextOperation;
	operation->extraNext = NULL;

	return variableDefinitionResult;
}

static IRResult buildIRFromSyntaxNode(KETLIRFunctionWIP* wip, KETLIROperation* rootOperation, KETLSyntaxNode* syntaxNodeRoot) {
	IRResult result;
	result.delegate = NULL;
	result.nextOperation = rootOperation;

	KETLSyntaxNode* it = syntaxNodeRoot;
	switch (it->type) {
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR: {
		KETLSyntaxNode* idNode = it->firstChild;
		IRConvertionResult variable = createVariableDefinition(wip, rootOperation, idNode, NULL);

		if (variable.variable == NULL) {
			return result;
		}

		result.delegate = wrapInDelegateValue(wip->builder, variable.variable);
		result.nextOperation = variable.nextOperation;

		return result;
	}
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE: {
		KETLSyntaxNode* typeNode = it->firstChild;

		KETLState* state = wip->builder->state;
		const char* typeName = ketlAtomicStringsGet(&state->strings, typeNode->value, typeNode->length);

		KETLType* type = ketlIntMapGet(&state->globalNamespace.types, (KETLIntMapKey)typeName);

		KETLSyntaxNode* idNode = typeNode->nextSibling;
		IRConvertionResult variable = createVariableDefinition(wip, rootOperation, idNode, type);

		if (variable.variable == NULL) {
			return result;
		}

		result.delegate = wrapInDelegateValue(wip->builder, variable.variable);
		result.nextOperation = variable.nextOperation;

		return result;
	}
	case KETL_SYNTAX_NODE_TYPE_ID: {
		const char* uniqName = ketlAtomicStringsGet(&wip->builder->state->strings, it->value, it->length);
		IRUndefinedValue** ppValue;
		if (ketlIntMapGetOrCreate(&wip->builder->variablesMap, (KETLIntMapKey)uniqName, &ppValue)) {
			// TODO error
			__debugbreak();
		}

		result.delegate = wrapInDelegateUValue(wip->builder, *ppValue);
		result.nextOperation = rootOperation;

		return result;
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

		result.delegate = wrapInDelegateValue(wip->builder, variable);
		result.nextOperation = rootOperation;

		return result;
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PROD:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_DIV:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_EQUAL:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_UNEQUAL: {
		KETLIRScopedVariable* resultVariable = createTempVariable(wip);

		KETLSyntaxNode* lhsNode = it->firstChild;
		IRResult lhs = buildIRFromSyntaxNode(wip, rootOperation, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRResult rhs = buildIRFromSyntaxNode(wip, lhs.nextOperation, rhsNode);

		ketlResetPool(&wip->builder->castingPool);
		BinaryOperatorDeduction deductionStruct = deduceInstructionCode2(wip->builder, it->type, lhs.delegate, rhs.delegate);
		if (deductionStruct.operator == NULL) {
			// TODO error
			__debugbreak();
		}

		IRConvertionResult lhsConvertion = convertValues(wip, rhs.nextOperation, deductionStruct.lhsCasting);
		IRConvertionResult rhsConvertion = convertValues(wip, lhsConvertion.nextOperation, deductionStruct.rhsCasting);

		KETLBinaryOperator deduction = *deductionStruct.operator;
		KETLIROperation* operation = rhsConvertion.nextOperation;
		operation->code = deduction.code;
		operation->argumentCount = 3;
		operation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 3);
		operation->arguments[1] = &lhsConvertion.variable->value;
		operation->arguments[2] = &rhsConvertion.variable->value;

		operation->arguments[0] = &resultVariable->variable.value;
		resultVariable->variable.type = deduction.outputType;
		resultVariable->variable.traits = deduction.outputTraits;

		result.delegate = wrapInDelegateValue(wip->builder, &resultVariable->variable);
		result.nextOperation = createOperation(wip->builder);

		operation->mainNext = result.nextOperation;
		operation->extraNext = NULL;

		return result;
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN: {
		KETLSyntaxNode* lhsNode = it->firstChild;
		IRResult lhs = buildIRFromSyntaxNode(wip, rootOperation, lhsNode);
		KETLSyntaxNode* rhsNode = lhsNode->nextSibling;
		IRResult rhs = buildIRFromSyntaxNode(wip, lhs.nextOperation, rhsNode);

		KETLIROperation* operation = rhs.nextOperation;

		operation->code = KETL_IR_CODE_ASSIGN_8_BYTES; // TODO choose from type
		operation->argumentCount = 2;
		operation->arguments = ketlGetNFreeObjectsFromPool(&wip->builder->argumentPointersPool, 2);
		operation->arguments[0] = &lhs.delegate->caller->variable->value; // TODO actual convertion from udelegate to correct type
		operation->arguments[1] = &rhs.delegate->caller->variable->value; // TODO actual convertion from udelegate to correct type

		result.delegate = wrapInDelegateValue(wip->builder, lhs.delegate->caller->variable);
		result.nextOperation = createOperation(wip->builder);

		operation->mainNext = result.nextOperation;
		operation->extraNext = NULL;

		return result;
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

			rootOperation = createOperation(wip->builder);

			assignOperation->mainNext = rootOperation;
			assignOperation->extraNext = NULL;
		}

		KETLIROperation* operation = rootOperation;
		operation->code = KETL_IR_CODE_RETURN;
		operation->argumentCount = 0;

		operation->mainNext = NULL;
		operation->extraNext = NULL;

		return result;
	}
	KETL_NODEFAULT();
	__debugbreak();
	}
	return result;
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

static KETLIROperation* buildIRBlock(KETLIRFunctionWIP* wip, KETLIROperation* rootOperation, KETLSyntaxNode* syntaxNode) {
	for (KETLSyntaxNode* it = syntaxNode; it; it = it->nextSibling) {
		switch (it->type) {
		case KETL_SYNTAX_NODE_TYPE_BLOCK: {

			KETLIRScopedVariable* savedStack = wip->currentStack;
			uint64_t scopeIndex = wip->scopeIndex++;

			rootOperation = buildIRBlock(wip, rootOperation, it->firstChild);

			restoreScopeContext(wip, savedStack, scopeIndex);
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
			KETLIRScopedVariable* currentStack = wip->currentStack;
			KETLIRScopedVariable* stackRoot = wip->stackRoot;

			wip->tempVariables = NULL;
			wip->localVariables = NULL;

			IRResult result = buildIRFromSyntaxNode(wip, rootOperation, it);
			rootOperation = result.nextOperation;

			restoreLocalSopeContext(wip, currentStack, stackRoot);
		}
		}
	}

	return rootOperation;
}

static void countOperationsAndArguments(KETLIRFunctionWIP* wip, KETLIntMap* operationReferMap, KETLIntMap* argumentsMap, KETLIROperation* rootOperation) {
	while(rootOperation != NULL) {
		uint64_t* newRefer;
		if (!ketlIntMapGetOrCreate(operationReferMap, (KETLIntMapKey)rootOperation, &newRefer)) {
			return;
		}
		*newRefer = ketlIntMapGetSize(operationReferMap) - 1;

		uint64_t* newIndex;
		for (uint64_t i = 0; i < rootOperation->argumentCount; ++i) {
			if (ketlIntMapGetOrCreate(argumentsMap, (KETLIntMapKey)rootOperation->arguments[i], &newIndex)) {
				*newIndex = ketlIntMapGetSize(argumentsMap) - 1;
			}
		}
		
		rootOperation = rootOperation->mainNext;
	}
}

static inline uint64_t bakeStackUsage(KETLIRFunctionWIP* wip) {
	// TODO align
	uint64_t currentStackOffset = 0;
	uint64_t maxStackOffset = 0;

	KETLIRScopedVariable* it = wip->stackRoot;
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

KETLIRFunction* ketlBuildIR(KETLType* returnType, KETLIRBuilder* irBuilder, KETLSyntaxNode* syntaxNodeRoot) {
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

	wip.scopeIndex = 0;

	KETLIROperation* rootOperation = createOperation(irBuilder);
	wip.rootOperation = rootOperation;
	buildIRBlock(&wip, rootOperation, syntaxNodeRoot);

	KETLIntMap operationReferMap;
	ketlInitIntMap(&operationReferMap, sizeof(uint64_t), 16);
	KETLIntMap argumentsMap;
	ketlInitIntMap(&argumentsMap, sizeof(uint64_t), 16);

	countOperationsAndArguments(&wip, &operationReferMap, &argumentsMap, rootOperation);
	uint64_t operationCount = ketlIntMapGetSize(&operationReferMap);
	uint64_t argumentsCount = ketlIntMapGetSize(&argumentsMap);

	uint64_t maxStackOffset = bakeStackUsage(&wip);

	KETLIRFunctionDefinition functionDefinition;

 	functionDefinition.function = malloc(sizeof(KETLIRFunction));

	functionDefinition.function->stackUsage = maxStackOffset;

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