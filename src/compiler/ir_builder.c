//🫖ketl
#include "ir_builder.h"

#include "ir_node.h"
#include "variable_impl.h"
#include "compiler/syntax_node.h"

#include "ketl_impl.h"
#include "containers/object_pool.h"
#include "containers/atomic_strings.h"
#include "ketl/type.h"
#include "assert.h"

#include <stdlib.h>

KETL_FORWARD(KETLType);

KETL_DEFINE(ketl_scoped_variable) {
	ketl_ir_variable variable;
	ketl_scoped_variable* parent;
	ketl_scoped_variable* nextSibling;
	ketl_scoped_variable* firstChild;
};

KETL_DEFINE(CastingOption) {
	ketl_cast_operator* operator;
	ketl_ir_variable* variable;
	uint64_t score;
	CastingOption* next;
};

KETL_DEFINE(ReturnCastingOption) {
	ketl_type_pointer type;
	ketl_variable_traits traits;
	uint64_t score;
	ReturnCastingOption* next;
};

void ketl_ir_builder_init(ketl_ir_builder* irBuilder, ketl_state* state) {
	irBuilder->state = state;

	ketl_int_map_init(&irBuilder->variablesMap, sizeof(ketl_ir_undefined_value*), 16);
	ketl_object_pool_init(&irBuilder->scopedVariablesPool, sizeof(ketl_scoped_variable), 16);
	ketl_object_pool_init(&irBuilder->variablesPool, sizeof(ketl_ir_variable), 16);
	ketl_object_pool_init(&irBuilder->operationsPool, sizeof(ketl_ir_operation), 16);
	ketl_object_pool_init(&irBuilder->udelegatePool, sizeof(ketl_ir_undefined_delegate), 16);
	ketl_object_pool_init(&irBuilder->uvaluePool, sizeof(ketl_ir_undefined_value), 16);
	ketl_object_pool_init(&irBuilder->argumentPointersPool, sizeof(ketl_ir_argument*), 32);
	ketl_object_pool_init(&irBuilder->argumentsPool, sizeof(ketl_ir_argument), 16);

	ketl_object_pool_init(&irBuilder->castingPool, sizeof(CastingOption), 16);
	ketl_object_pool_init(&irBuilder->castingReturnPool, sizeof(ReturnCastingOption), 16);

	ketl_int_map_init(&irBuilder->operationReferMap, sizeof(uint64_t), 16);
	ketl_int_map_init(&irBuilder->argumentsMap, sizeof(uint64_t), 16);
	ketl_stack_init(&irBuilder->extraNextStack, sizeof(ketl_ir_operation*), 16);

	ketl_object_pool_init(&irBuilder->returnPool, sizeof(ketl_ir_return_node), 16);
}

void ketl_ir_builder_deinit(ketl_ir_builder* irBuilder) {
	ketl_object_pool_deinit(&irBuilder->returnPool);

	ketl_stack_deinit(&irBuilder->extraNextStack);
	ketl_int_map_deinit(&irBuilder->argumentsMap);
	ketl_int_map_deinit(&irBuilder->operationReferMap);

	ketl_object_pool_deinit(&irBuilder->castingReturnPool);
	ketl_object_pool_deinit(&irBuilder->castingPool);

	ketl_object_pool_deinit(&irBuilder->argumentsPool);
	ketl_object_pool_deinit(&irBuilder->argumentPointersPool);
	ketl_object_pool_deinit(&irBuilder->uvaluePool);
	ketl_object_pool_deinit(&irBuilder->udelegatePool);
	ketl_object_pool_deinit(&irBuilder->operationsPool);
	ketl_object_pool_deinit(&irBuilder->variablesPool);
	ketl_object_pool_deinit(&irBuilder->scopedVariablesPool);
	ketl_int_map_deinit(&irBuilder->variablesMap);
}


static inline ketl_ir_undefined_delegate* wrapInDelegateUValue(ketl_ir_builder* irBuilder, ketl_ir_undefined_value* uvalue) {
	ketl_ir_undefined_delegate* udelegate = ketl_object_pool_get(&irBuilder->udelegatePool);
	udelegate->caller = uvalue;
	udelegate->arguments = NULL;
	udelegate->next = NULL;

	return udelegate;
}

static inline ketl_ir_undefined_value* wrapInUValueVariable(ketl_ir_builder* irBuilder, ketl_ir_variable* variable) {
	ketl_ir_undefined_value* uvalue = ketl_object_pool_get(&irBuilder->uvaluePool);
	uvalue->variable = variable;
	uvalue->next = NULL;

	return uvalue;
}

static inline ketl_ir_undefined_delegate* wrapInDelegateValue(ketl_ir_builder* irBuilder, ketl_ir_variable* variable) {
	return wrapInDelegateUValue(irBuilder, wrapInUValueVariable(irBuilder, variable));
}

static inline ketl_ir_operation* createOperationImpl(ketl_ir_builder* irBuilder) {
	// TODO check recycled operations
	return ketl_object_pool_get(&irBuilder->operationsPool);
}

static inline void recycleOperationImpl(ketl_ir_builder* irBuilder, ketl_ir_operation* operation) {
	(void)irBuilder;
	(void)operation;
	// TODO recycle
}

static inline void initInnerOperationRange(ketl_ir_builder* irBuilder, ketl_ir_operation_range* baseRange, ketl_ir_operation_range* innerRange) {
	innerRange->root = baseRange->root;
	innerRange->next = createOperationImpl(irBuilder);
}

static inline ketl_ir_operation* createOperationFromRange(ketl_ir_builder* irBuilder, ketl_ir_operation_range* range) {
	ketl_ir_operation* operation = range->root;
	operation->mainNext = range->root = range->next;
	operation->extraNext = NULL;
	range->next = createOperationImpl(irBuilder);
	return operation;
}

static inline ketl_ir_operation* createLastOperationFromRange(ketl_ir_operation_range* range) {
	ketl_ir_operation* operation = range->root;
	operation->extraNext = operation->mainNext = range->root = NULL;
	return operation;
}

static inline void initSideOperationRange(ketl_ir_builder* irBuilder, ketl_ir_operation_range* baseRange, ketl_ir_operation_range* sideRange) {
	sideRange->root = createOperationImpl(irBuilder);
	sideRange->next = baseRange->next;
}

static inline void deinitSideOperationRange(ketl_ir_builder* irBuilder, ketl_ir_operation_range* sideRange) {
	recycleOperationImpl(irBuilder, sideRange->next);
}

static inline void deinitInnerOperationRange(ketl_ir_builder* irBuilder, ketl_ir_operation_range* baseRange, ketl_ir_operation_range* innerRange) {
	baseRange->root = innerRange->root;
	recycleOperationImpl(irBuilder, innerRange->next);
}

static inline ketl_scoped_variable* createTempVariable(KETLIRFunctionWIP* wip) {
	ketl_scoped_variable* stackValue = ketl_object_pool_get(&wip->builder->scopedVariablesPool);
	stackValue->variable.value.type = KETL_IR_ARGUMENT_TYPE_STACK;
	stackValue->variable.value.stack = 0;
	stackValue->firstChild = NULL;
	stackValue->nextSibling = NULL;

	ketl_scoped_variable* tempVariables = wip->vars.tempVariables;
	if (tempVariables != NULL) {
		stackValue->parent = tempVariables;
		tempVariables->firstChild = stackValue;
	}
	else {
		stackValue->parent = NULL;
	}
	wip->vars.tempVariables = stackValue;
	return stackValue;
}

static inline ketl_scoped_variable* createLocalVariable(KETLIRFunctionWIP* wip) {
	ketl_scoped_variable* stackValue = ketl_object_pool_get(&wip->builder->scopedVariablesPool);
	stackValue->variable.value.type = KETL_IR_ARGUMENT_TYPE_STACK;
	stackValue->variable.value.stack = 0;
	stackValue->firstChild = NULL;
	stackValue->nextSibling = NULL;

	ketl_scoped_variable* localVariables = wip->vars.localVariables;
	if (localVariables != NULL) {
		stackValue->parent = localVariables;
		localVariables->firstChild = stackValue;
	}
	else {
		stackValue->parent = NULL;
	}
	wip->vars.localVariables = stackValue;
	return stackValue;
}

KETL_DEFINE(Literal) {
	ketl_type_pointer type;
	ketl_ir_argument value;
};

static inline Literal parseLiteral(KETLIRFunctionWIP* wip, const char* value, size_t length) {
	Literal literal;
	int64_t intValue = ketl_str_to_i64(value, length);
	if (INT8_MIN <= intValue && intValue <= INT8_MAX) {
		literal.type.primitive = &wip->builder->state->primitives.i8_t;
		literal.value.int8 = (int8_t)intValue;
		literal.value.type = KETL_IR_ARGUMENT_TYPE_INT8;
	}
	else if (INT16_MIN <= intValue && intValue <= INT16_MAX) {
		literal.type.primitive = &wip->builder->state->primitives.i16_t;
		literal.value.int16 = (int16_t)intValue;
		literal.value.type = KETL_IR_ARGUMENT_TYPE_INT16;
	}
	else if (INT32_MIN <= intValue && intValue <= INT32_MAX) {
		literal.type.primitive = &wip->builder->state->primitives.i32_t;
		literal.value.int32 = (int32_t)intValue;
		literal.value.type = KETL_IR_ARGUMENT_TYPE_INT32;
	}
	else if (INT64_MIN <= intValue && intValue <= INT16_MAX) {
		literal.type.primitive = &wip->builder->state->primitives.i64_t;
		literal.value.int64 = intValue;
		literal.value.type = KETL_IR_ARGUMENT_TYPE_INT64;
	}
	return literal;
}

static inline bool isLiteralType(ketl_ir_argument* argument, ketl_type_pointer targetType, ketl_state* state) {
	if (targetType.base->kind != KETL_TYPE_KIND_PRIMITIVE) {
		return false;
	}
	if (targetType.primitive == &state->primitives.void_t
		|| targetType.primitive == &state->primitives.bool_t) {
		return false;
	}
	if (argument->type < KETL_IR_ARGUMENT_TYPE_INT8) {
		return false;
	}
	return true;
}

static inline void convertLiteralSize(ketl_ir_variable* variable, ketl_type_pointer targetType, ketl_state* state) {
	bool floatFlag = false;
	uint64_t intValue;
	double floatValue;


	switch (variable->value.type) {
	case KETL_IR_ARGUMENT_TYPE_INT8:
		intValue = variable->value.int8;
		break;
	case KETL_IR_ARGUMENT_TYPE_INT16:
		intValue = variable->value.int16;
		break;
	case KETL_IR_ARGUMENT_TYPE_INT32:
		intValue = variable->value.int32;
		break;
	case KETL_IR_ARGUMENT_TYPE_INT64:
		intValue = variable->value.int64;
		break;

	case KETL_IR_ARGUMENT_TYPE_UINT8:
		intValue = variable->value.uint8;
		break;
	case KETL_IR_ARGUMENT_TYPE_UINT16:
		intValue = variable->value.uint16;
		break;
	case KETL_IR_ARGUMENT_TYPE_UINT32:
		intValue = variable->value.uint32;
		break;
	case KETL_IR_ARGUMENT_TYPE_UINT64:
		intValue = variable->value.uint64;
		break;

	case KETL_IR_ARGUMENT_TYPE_FLOAT32:
		floatFlag = true;
		floatValue = variable->value.float32;
		break;
	case KETL_IR_ARGUMENT_TYPE_FLOAT64:
		floatFlag = true;
		floatValue = variable->value.float64;
		break;
	KETL_NODEFAULT()
	}

	switch (targetType.primitive->size) {
	case 1: 
		if (targetType.primitive == &state->primitives.i8_t) {
			if (!floatFlag) {
				variable->value.int8 = intValue;
			} else {
				variable->value.int8 = floatValue;
			}
		} else /* if (targetType.primitive == &state->primitives.u8_t) */ {
			if (!floatFlag) {
				variable->value.uint8 = intValue;
			} else {
				variable->value.uint8 = floatValue;
			}
		}
		return;
	case 2: 
		if (targetType.primitive == &state->primitives.i16_t) {
			if (!floatFlag) {
				variable->value.int16 = intValue;
			} else {
				variable->value.int16 = floatValue;
			}
		} else /* if (targetType.primitive == &state->primitives.u16_t) */ {
			if (!floatFlag) {
				variable->value.uint16 = intValue;
			} else {
				variable->value.uint16 = floatValue;
			}
		}
		return;
	case 4:
		if (targetType.primitive == &state->primitives.i32_t) {
			if (!floatFlag) {
				variable->value.int32 = intValue;
			} else {
				variable->value.int32 = floatValue;
			}
		} else if (targetType.primitive == &state->primitives.u32_t) {
			if (!floatFlag) {
				variable->value.uint32 = intValue;
			} else {
				variable->value.uint32 = floatValue;
			}
		} else /* if (targetType.primitive == &state->primitives.f32_t) */ {
			if (!floatFlag) {
				variable->value.float32 = intValue;
			} else {
				variable->value.float32 = floatValue;
			}
		}
		return;
	case 8:
		if (targetType.primitive == &state->primitives.i64_t) {
			if (!floatFlag) {
				variable->value.int64 = intValue;
			} else {
				variable->value.int64 = floatValue;
			}
		} else if (targetType.primitive == &state->primitives.u64_t) {
			if (!floatFlag) {
				variable->value.uint64 = intValue;
			} else {
				variable->value.uint64 = floatValue;
			}
		} else /* if (targetType.primitive == &state->primitives.f64_t) */ {
			if (!floatFlag) {
				variable->value.float64 = intValue;
			} else {
				variable->value.float64 = floatValue;
			}
		}
		return;
	KETL_NODEFAULT()
	}
}

static void convertValues(KETLIRFunctionWIP* wip, ketl_ir_operation_range* operationRange, CastingOption* castingOption) {
	ketl_ir_variable* variable = castingOption->variable;
	ketl_type_pointer targetType = castingOption->operator ? castingOption->operator->outputType : castingOption->variable->type;
	if (isLiteralType(&variable->value, targetType, wip->builder->state)) {
		convertLiteralSize(variable, targetType, wip->builder->state);
		variable->type = targetType;
	}
	else {
		if (castingOption == NULL || castingOption->operator == NULL) {
			return;
		}

		ketl_ir_variable* result = &createTempVariable(wip)->variable;

		ketl_cast_operator casting = *castingOption->operator;
		ketl_ir_operation* convertOperation = createOperationFromRange(wip->builder, operationRange);
		convertOperation->code = casting.code;
		convertOperation->argumentCount = 2;
		convertOperation->arguments = ketl_object_pool_get_array(&wip->builder->argumentPointersPool, 2);
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

static TypeCastingTargetList possibleCastingForValue(ketl_ir_builder* irBuilder, ketl_ir_variable* variable, bool implicit) {
	TypeCastingTargetList targets;
	targets.begin = targets.last = NULL;

	bool numberLiteral = isLiteralType(&variable->value, variable->type, irBuilder->state);
#define MAX_SIZE_OF_NUMBER_LITERAL 8

	ketl_cast_operator** castOperators = ketl_int_map_get(&irBuilder->state->castOperators, (ketl_int_map_key)variable->type.base);
	if (castOperators != NULL) {
		ketl_cast_operator* it = *castOperators;
		for (; it; it = it->next) {
			if (implicit && !it->implicit) {
				continue;
			}

			// TODO check traits

			CastingOption* newTarget = ketl_object_pool_get(&irBuilder->castingPool);
			newTarget->variable = variable;
			newTarget->operator = it;

			if (targets.last == NULL) {
				targets.last = newTarget;
			}
			newTarget->next = targets.begin;
			targets.begin = newTarget;

			newTarget->score = !numberLiteral || it->outputType.primitive->size != MAX_SIZE_OF_NUMBER_LITERAL;
		}
	}

	CastingOption* newTarget = ketl_object_pool_get(&irBuilder->castingPool);
	newTarget->variable = variable;
	newTarget->operator = NULL;

	if (targets.last == NULL) {
		targets.last = newTarget;
	}
	newTarget->next = targets.begin;
	targets.begin = newTarget;

	newTarget->score = numberLiteral && variable->type.primitive->size != MAX_SIZE_OF_NUMBER_LITERAL;

	return targets;
}

static CastingOption* possibleCastingForDelegate(ketl_ir_builder* irBuilder, ketl_ir_undefined_delegate* udelegate) {
	CastingOption* targets = NULL;

	if (udelegate == NULL) {
		return NULL;
	}

	ketl_ir_undefined_value* callerIt = udelegate->caller;
	for (; callerIt; callerIt = callerIt->next) {
		ketl_ir_variable* callerValue = callerIt->variable;
		ketl_type_pointer callerType = callerValue->type;


		ketl_ir_variable* outputValue = callerValue;

		if (callerType.base->kind == KETL_TYPE_KIND_FUNCTION) {
			KETL_DEBUGBREAK();
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
	ketl_binary_operator* operator;
	CastingOption* lhsCasting;
	CastingOption* rhsCasting;
};

static BinaryOperatorDeduction deduceInstructionCode2(ketl_ir_builder* irBuilder, ketl_syntax_node_type syntaxOperator, ketl_ir_undefined_delegate* lhs, ketl_ir_undefined_delegate* rhs) {
	BinaryOperatorDeduction result;
	result.operator = NULL;
	result.lhsCasting = NULL;
	result.rhsCasting = NULL;

	ketl_operator_code operatorCode = syntaxOperator - KETL_SYNTAX_NODE_TYPE_OPERATOR_OFFSET;
	ketl_state* state = irBuilder->state;

	ketl_binary_operator** pOperatorResult = ketl_int_map_get(&state->binaryOperators, operatorCode);
	if (pOperatorResult == NULL) {
		return result;
	}
	ketl_binary_operator* operatorResult = *pOperatorResult;

	uint64_t bestScore = UINT64_MAX;

	uint64_t currentScore;

	CastingOption* lhsCasting = possibleCastingForDelegate(irBuilder, lhs);
	CastingOption* rhsCasting = possibleCastingForDelegate(irBuilder, rhs);

	ketl_binary_operator* it = operatorResult;
	for (; it; it = it->next) {
		currentScore = 0;
		CastingOption* lhsIt = lhsCasting;
		for (; lhsIt; lhsIt = lhsIt->next) {
			ketl_type_pointer lhsType = lhsIt->operator ? lhsIt->operator->outputType : lhsIt->variable->type;
			if (it->lhsType.base != lhsType.base) {
				continue;
			}

			CastingOption* rhsIt = rhsCasting;
			for (; rhsIt; rhsIt = rhsIt->next) {
				ketl_type_pointer rhsType = rhsIt->operator ? rhsIt->operator->outputType : rhsIt->variable->type;
				if (it->rhsType.base != rhsType.base) {
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

static CastingOption* castDelegateToVariable(ketl_ir_builder* irBuilder, ketl_ir_undefined_delegate* udelegate, ketl_type_pointer type) {
	CastingOption* options = possibleCastingForDelegate(irBuilder, udelegate);
	CastingOption* it = options;

	for (; it; it = it->next) {
		ketl_type_pointer castingType = it->operator ? it->operator->outputType : it->variable->type;
		if (castingType.base == type.base) {
			return it;
		}
	}

	return NULL;
}

static CastingOption* getBestCastingOptionForDelegate(ketl_ir_builder* irBuilder, ketl_ir_undefined_delegate* udelegate) {
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

static ketl_ir_undefined_delegate* buildIRCommandTree(KETLIRFunctionWIP* wip, ketl_ir_operation_range* operationRange, ketl_syntax_node* syntaxNodeRoot) {
	ketl_syntax_node* it = syntaxNodeRoot;
	switch (it->type) {
	case KETL_SYNTAX_NODE_TYPE_ID: {
		const char* uniqName = ketl_atomic_strings_get(&wip->builder->state->strings, it->value, it->length);
		ketl_ir_undefined_value** ppValue = ketl_int_map_get(&wip->builder->variablesMap, (ketl_int_map_key)uniqName);
		if (ppValue == NULL) {
			ppValue = ketl_int_map_get(&wip->builder->state->globalNamespace.variables, (ketl_int_map_key)uniqName);
		}
		if (ppValue == NULL) {
			KETL_ERROR("Unknown variable '%s' at '%u:%u'", 
				uniqName, it->lineInSource, it->columnInSource);
			wip->buildFailed = true;
			return NULL;
		}

		return wrapInDelegateUValue(wip->builder, *ppValue);
	}
	case KETL_SYNTAX_NODE_TYPE_NUMBER: {
		Literal literal = parseLiteral(wip, it->value, it->length);
		if (literal.type.base == NULL) {
			KETL_ERROR("Can't parse literal number '%.*s' at '%u:%u'", 
				it->value, it->length, it->lineInSource, it->columnInSource);
			wip->buildFailed = true;
			return NULL;
		}
		ketl_ir_variable* variable = ketl_object_pool_get(&wip->builder->variablesPool);
		variable->type = literal.type;
		variable->traits.values.isNullable = false;
		variable->traits.values.isConst = true;
		variable->traits.values.type = KETL_TRAIT_TYPE_LITERAL;
		variable->value = literal.value;

		return wrapInDelegateValue(wip->builder, variable);
	}
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PROD:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_DIV:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_EQUAL:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_UNEQUAL:
	case KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN: {
		ketl_ir_operation_range innerRange;
		initInnerOperationRange(wip->builder, operationRange, &innerRange);
		ketl_scoped_variable* resultVariable = createTempVariable(wip);

		ketl_syntax_node* lhsNode = it->firstChild;
		ketl_ir_undefined_delegate* lhs = buildIRCommandTree(wip, &innerRange, lhsNode);
		ketl_syntax_node* rhsNode = lhsNode->nextSibling;
		ketl_ir_undefined_delegate* rhs = buildIRCommandTree(wip, &innerRange, rhsNode);

		ketl_object_pool_reset(&wip->builder->castingPool);
		BinaryOperatorDeduction deductionStruct = deduceInstructionCode2(wip->builder, it->type, lhs, rhs);
		if (deductionStruct.operator == NULL) {
			// TODO get operator symbols from type
			const char* opSymbols = "opPlaceholder";
			KETL_ERROR("Can't deduce operator call for '%s' at '%u:%u'",
				opSymbols, it->lineInSource, it->columnInSource);
			wip->buildFailed = true;
			return NULL;
		}

		convertValues(wip, &innerRange, deductionStruct.lhsCasting);
		convertValues(wip, &innerRange, deductionStruct.rhsCasting);

		deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

		ketl_binary_operator deduction = *deductionStruct.operator;
		ketl_ir_operation* operation = createOperationFromRange(wip->builder, operationRange);
		operation->code = deduction.code;
		operation->argumentCount = 3;
		operation->arguments = ketl_object_pool_get_array(&wip->builder->argumentPointersPool, 3);
		operation->arguments[1] = &deductionStruct.lhsCasting->variable->value;
		operation->arguments[2] = &deductionStruct.rhsCasting->variable->value;

		operation->arguments[0] = &resultVariable->variable.value;
		resultVariable->variable.type = deduction.outputType;
		resultVariable->variable.traits = deduction.outputTraits;

		operation->extraNext = NULL;

		return wrapInDelegateValue(wip->builder, &resultVariable->variable);;
	}
	KETL_NODEFAULT();
	KETL_DEBUGBREAK();
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
		ketl_scoped_variable* lastChild = currentStack->firstChild;\
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
			ketl_scoped_variable* lastChild = stackRoot;\
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

static inline void restoreLocalScopeContext(KETLIRFunctionWIP* wip, ketl_scoped_variable* currentStack, ketl_scoped_variable* stackRoot) {
	{
		// set local variable
		ketl_scoped_variable* firstLocalVariable = wip->vars.localVariables;
		if (firstLocalVariable != NULL) {
			// TODO remember, why this line was even here
			//ketl_scoped_variable* lastLocalVariables = firstLocalVariable;
			UPDATE_NODE_TALE(firstLocalVariable);
			currentStack = firstLocalVariable;
		}
	}

	{
		// set temp variable
		ketl_scoped_variable* firstTempVariable = wip->vars.tempVariables;
		if (firstTempVariable) {
			UPDATE_NODE_TALE(firstTempVariable);
		}
	}

	wip->stack.currentStack = currentStack;
	wip->stack.stackRoot = stackRoot;
}

static inline void restoreScopeContext(KETLIRFunctionWIP* wip, ketl_scoped_variable* savedStack, uint64_t scopeIndex) {
	wip->scopeIndex = scopeIndex;
	wip->stack.currentStack = savedStack;

	ketl_int_map_iterator iterator;
	ketl_int_map_iterator_init(&iterator, &wip->builder->variablesMap);

	while (ketl_int_map_iterator_has_next(&iterator)) {
		const char* name;
		ketl_ir_undefined_value** pCurrent;
		ketl_int_map_iterator_get(&iterator, (ketl_int_map_key*)&name, &pCurrent);
		ketl_ir_undefined_value* current = *pCurrent;
		KETL_FOREVER{
			if (current == NULL) {
				ketl_int_map_iterator_remove(&iterator);
				break;
			}

			if (current->scopeIndex <= scopeIndex) {
				*pCurrent = current;
				ketl_int_map_iterator_next(&iterator);
				break;
			}

			current = current->next;
		}
	}
}

static void createVariableDefinition(KETLIRFunctionWIP* wip, ketl_ir_operation_range* operationRange, ketl_syntax_node* idNode, ketl_type_pointer type) {
	if (KETL_CHECK_VOE(idNode->type == KETL_SYNTAX_NODE_TYPE_ID)) {
		return;
	}

	ketl_ir_operation_range innerRange;
	initInnerOperationRange(wip->builder, operationRange, &innerRange);

	ketl_scoped_variable* variable = createLocalVariable(wip);

	ketl_syntax_node* expressionNode = idNode->nextSibling;
	ketl_ir_undefined_delegate* expression = buildIRCommandTree(wip, &innerRange, expressionNode);

	const char* name = ketl_atomic_strings_get(&wip->builder->state->strings, idNode->value, idNode->length);

	uint64_t scopeIndex = wip->scopeIndex;

	ketl_ir_undefined_value** pCurrent;
	if (ketl_int_map_get_or_create(&wip->builder->variablesMap, (ketl_int_map_key)name, &pCurrent)) {
		*pCurrent = NULL;
	}
	else {
		// TODO check if the type is function, then we can overload
		if ((*pCurrent)->scopeIndex == scopeIndex) {
			KETL_ERROR("Variable redefinition '%s' at '%u:%u'", 
				name, idNode->lineInSource, idNode->columnInSource);
			wip->buildFailed = true;
			return;
		}
	}
	ketl_ir_undefined_value* uvalue = wrapInUValueVariable(wip->builder, &variable->variable);
	uvalue->scopeIndex = scopeIndex;
	uvalue->next = *pCurrent;
	*pCurrent = uvalue;

	if (expression == NULL) {
		// TODO default initialization
		KETL_DEBUGBREAK();
	}

	// TODO set traits properly from syntax node
	variable->variable.traits.values.isConst = false;
	variable->variable.traits.values.isNullable = false;
	variable->variable.traits.values.type = KETL_TRAIT_TYPE_LVALUE;

	CastingOption* expressionCasting;

	ketl_object_pool_reset(&wip->builder->castingPool);
	if (type.base == NULL) {
		expressionCasting = getBestCastingOptionForDelegate(wip->builder, expression);
	}
	else {
		expressionCasting = castDelegateToVariable(wip->builder, expression, type);

		if (expressionCasting == NULL) {
			KETL_ERROR("Can't cast expression to type '%s' at '%u:%u'", 
				type.base->name, expressionNode->lineInSource, expressionNode->columnInSource);
			wip->buildFailed = true;
			return;
		}
	}

	convertValues(wip, &innerRange, expressionCasting);

	deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

	ketl_ir_variable* expressionVarible = expressionCasting->variable;
	variable->variable.type = expressionVarible->type;

	ketl_ir_operation* operation = createOperationFromRange(wip->builder, operationRange);

	switch (ketl_type_get_stack_size(variable->variable.traits, variable->variable.type)) {
	case 1:
		operation->code = KETL_IR_CODE_COPY_1_BYTE;
		break;
	case 2:
		operation->code = KETL_IR_CODE_COPY_2_BYTES;
		break;
	case 4:
		operation->code = KETL_IR_CODE_COPY_4_BYTES;
		break;
	case 8:
		operation->code = KETL_IR_CODE_COPY_8_BYTES;
		break;
	default:
		KETL_DEBUGBREAK();
	}

	operation->argumentCount = 2;
	operation->arguments = ketl_object_pool_get_array(&wip->builder->argumentPointersPool, 2);
	operation->arguments[0] = &variable->variable.value;
	operation->arguments[1] = &expressionVarible->value;

	operation->extraNext = NULL;
}

static void buildIRCommandLoopIteration(KETLIRFunctionWIP* wip, ketl_ir_operation_range* operationRange, ketl_syntax_node* syntaxNode);

static void buildIRCommand(KETLIRFunctionWIP* wip, ketl_ir_operation_range* operationRange, ketl_syntax_node* syntaxNode) {
	bool isNull = syntaxNode == NULL;
	if (isNull) {
		return;
	}
	bool nonLast = syntaxNode->nextSibling != NULL;
	if (!nonLast) {
		buildIRCommandLoopIteration(wip, operationRange, syntaxNode);
		return;
	}

	ketl_ir_operation_range innerRange;
	initInnerOperationRange(wip->builder, operationRange, &innerRange);

	do {
		buildIRCommandLoopIteration(wip, &innerRange, syntaxNode);
		syntaxNode = syntaxNode->nextSibling;
		nonLast = syntaxNode->nextSibling != NULL;
	} while (nonLast);

	deinitInnerOperationRange(wip->builder, operationRange, &innerRange);
	buildIRCommandLoopIteration(wip, operationRange, syntaxNode);
}

static void buildIRCommandLoopIteration(KETLIRFunctionWIP* wip, ketl_ir_operation_range* operationRange, ketl_syntax_node* syntaxNode) {
	switch (syntaxNode->type) {
	case KETL_SYNTAX_NODE_TYPE_BLOCK: {
		ketl_scoped_variable* savedStack = wip->stack.currentStack;
		uint64_t scopeIndex = wip->scopeIndex++;

		buildIRCommand(wip, operationRange, syntaxNode->firstChild);

		restoreScopeContext(wip, savedStack, scopeIndex);
		break;
	}
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR: {
		ketl_scoped_variable* currentStack = wip->stack.currentStack;
		ketl_scoped_variable* stackRoot = wip->stack.stackRoot;

		wip->vars.tempVariables = NULL;
		wip->vars.localVariables = NULL;

		ketl_syntax_node* idNode = syntaxNode->firstChild;

		createVariableDefinition(wip, operationRange, idNode, KETL_CREATE_TYPE_PTR(NULL));

		restoreLocalScopeContext(wip, currentStack, stackRoot);
		break;
	}
	case KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE: {
		ketl_scoped_variable* currentStack = wip->stack.currentStack;
		ketl_scoped_variable* stackRoot = wip->stack.stackRoot;

		wip->vars.tempVariables = NULL;
		wip->vars.localVariables = NULL;

		ketl_syntax_node* typeNode = syntaxNode->firstChild;

		ketl_state* state = wip->builder->state;
		const char* typeName = ketl_atomic_strings_get(&state->strings, typeNode->value, typeNode->length);

		ketl_type_pointer type = *(ketl_type_pointer*)ketl_int_map_get(&state->globalNamespace.types, (ketl_int_map_key)typeName);

		ketl_syntax_node* idNode = typeNode->nextSibling;

		createVariableDefinition(wip, operationRange, idNode, type);

		restoreLocalScopeContext(wip, currentStack, stackRoot);
		break;
	}
	case KETL_SYNTAX_NODE_TYPE_IF_ELSE: {
		ketl_scoped_variable* savedStack = wip->stack.currentStack;
		uint64_t scopeIndex = wip->scopeIndex++;

		ketl_scoped_variable* currentStack = wip->stack.currentStack;
		ketl_scoped_variable* stackRoot = wip->stack.stackRoot;

		wip->vars.tempVariables = NULL;
		wip->vars.localVariables = NULL;

		ketl_ir_operation_range innerRange;
		initInnerOperationRange(wip->builder, operationRange, &innerRange);

		ketl_syntax_node* expressionNode = syntaxNode->firstChild;
		ketl_ir_undefined_delegate* expression = buildIRCommandTree(wip, &innerRange, expressionNode);

		// TODO find more clever way to have typePtr literals or at least make inline function
		CastingOption* expressionCasting = castDelegateToVariable(wip->builder, expression, KETL_CREATE_TYPE_PTR(&wip->builder->state->primitives.bool_t));
		
		if (expressionCasting == NULL) {
			KETL_ERROR("Can't cast expression to type '%s' at '%u:%u'", 
				wip->builder->state->primitives.bool_t.name, expressionNode->lineInSource, expressionNode->columnInSource);
			wip->buildFailed = true;
			restoreLocalScopeContext(wip, currentStack, stackRoot);
			restoreScopeContext(wip, savedStack, scopeIndex);
			return;
		}
		convertValues(wip, &innerRange, expressionCasting);

		restoreLocalScopeContext(wip, currentStack, stackRoot);

		ketl_ir_operation* ifJumpOperation = createOperationFromRange(wip->builder, &innerRange);
		ifJumpOperation->code = KETL_IR_CODE_JUMP_IF_FALSE;
		ifJumpOperation->argumentCount = 1;
		ifJumpOperation->arguments = ketl_object_pool_get_array(&wip->builder->argumentPointersPool, 1);
		ifJumpOperation->arguments[0] = &expressionCasting->variable->value;

		deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

		// builds true instructions
		ketl_syntax_node* trueBlockNode = expressionNode->nextSibling;
		ketl_syntax_node* falseBlockNode = trueBlockNode->nextSibling;

		if (falseBlockNode == NULL) {
			ifJumpOperation->extraNext = operationRange->next;

			// builds true instructions
			ketl_syntax_node* trueBlockNode = expressionNode->nextSibling;
			ketl_ir_operation* checkTrueRoot = operationRange->root;
			buildIRCommandLoopIteration(wip, operationRange, trueBlockNode);
			ketl_ir_operation* newRoot = operationRange->root;
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
			ketl_ir_operation_range sideRange;
			initSideOperationRange(wip->builder, operationRange, &sideRange);

			ifJumpOperation->extraNext = sideRange.root;

			// builds true instructions
			ketl_ir_operation* checkTrueRoot = operationRange->root;
			buildIRCommandLoopIteration(wip, operationRange, trueBlockNode);
			ketl_ir_operation* newRoot = operationRange->root;
			// true block is empty
			if (checkTrueRoot == newRoot) {
				operationRange->root = operationRange->next;
				operationRange->next = newRoot;

				ifJumpOperation->mainNext = operationRange->root;

				// builds false instructions
				buildIRCommandLoopIteration(wip, &sideRange, falseBlockNode);
				// TODO check if false block is empty
				deinitSideOperationRange(wip->builder, &sideRange);
			}
			// true block has return
			else if (NULL == newRoot) {
				// builds false instructions
				buildIRCommandLoopIteration(wip, &sideRange, falseBlockNode);
				*operationRange = sideRange;
			}
			else {
				// builds false instructions
				buildIRCommandLoopIteration(wip, &sideRange, falseBlockNode);
				// TODO check if false block is empty
				deinitSideOperationRange(wip->builder, &sideRange);
			}
		}

		restoreScopeContext(wip, savedStack, scopeIndex);
		break;
	}
	case KETL_SYNTAX_NODE_TYPE_RETURN: {
		ketl_syntax_node* expressionNode = syntaxNode->firstChild;
		if (expressionNode) {
			ketl_scoped_variable* currentStack = wip->stack.currentStack;
			ketl_scoped_variable* stackRoot = wip->stack.stackRoot;

			wip->vars.tempVariables = NULL;
			wip->vars.localVariables = NULL;

			ketl_ir_operation_range innerRange;
			initInnerOperationRange(wip->builder, operationRange, &innerRange);
			ketl_ir_undefined_delegate* expression = buildIRCommandTree(wip, &innerRange, expressionNode);
			deinitInnerOperationRange(wip->builder, operationRange, &innerRange);

			ketl_ir_operation* returnOperation = createLastOperationFromRange(operationRange);
			returnOperation->code = KETL_IR_CODE_RETURN_8_BYTES; // TODO deside from type
			returnOperation->argumentCount = 1;

			ketl_ir_return_node* returnNode = ketl_object_pool_get(&wip->builder->returnPool);
			returnNode->next = wip->returnOperations;
			returnNode->operation = returnOperation;
			returnNode->returnVariable = expression;
			returnNode->tempVariable = wip->vars.tempVariables;
			wip->returnOperations = returnNode;

			restoreLocalScopeContext(wip, currentStack, stackRoot);
		}
		else {
			ketl_ir_operation* returnOperation = createLastOperationFromRange(operationRange);
			returnOperation->code = KETL_IR_CODE_RETURN;
			returnOperation->argumentCount = 0;

			ketl_ir_return_node* returnNode = ketl_object_pool_get(&wip->builder->returnPool);
			returnNode->next = wip->returnOperations;
			returnNode->operation = returnOperation;
			returnNode->returnVariable = NULL;
			returnNode->tempVariable = NULL;
			wip->returnOperations = returnNode;
		}

		break;
	}
	default: {
		ketl_scoped_variable* currentStack = wip->stack.currentStack;
		ketl_scoped_variable* stackRoot = wip->stack.stackRoot;

		wip->vars.tempVariables = NULL;
		wip->vars.localVariables = NULL;

		buildIRCommandTree(wip, operationRange, syntaxNode);

		restoreLocalScopeContext(wip, currentStack, stackRoot);
	}
	}
}

static void countOperationsAndArguments(ketl_ir_operation* rootOperation, ketl_ir_builder* irBuilder) {
	while(rootOperation != NULL) {
		uint64_t* newRefer;
		if (!ketl_int_map_get_or_create(&irBuilder->operationReferMap, (ketl_int_map_key)rootOperation, &newRefer)) {
			return;
		}
		*newRefer = ketl_int_map_get_size(&irBuilder->operationReferMap) - 1;

		uint64_t* newIndex;
		for (uint64_t i = 0; i < rootOperation->argumentCount; ++i) {
			if (ketl_int_map_get_or_create(&irBuilder->argumentsMap, (ketl_int_map_key)rootOperation->arguments[i], &newIndex)) {
				*newIndex = ketl_int_map_get_size(&irBuilder->argumentsMap) - 1;
			}
		}
		
		ketl_ir_operation* extraNext = rootOperation->extraNext;
		if (extraNext != NULL) {
			*(ketl_ir_operation**)ketl_stack_push(&irBuilder->extraNextStack) = extraNext;
		}
		rootOperation = rootOperation->mainNext;
	}
}

static inline uint64_t bakeStackUsage(ketl_scoped_variable* stackRoot) {
	// TODO align
	uint64_t currentStackOffset = 0;
	uint64_t maxStackOffset = 0;

	ketl_scoped_variable* it = stackRoot;
	if (it == NULL) {
		return maxStackOffset;
	}

	KETL_FOREVER{
		it->variable.value.stack = currentStackOffset;

		uint64_t size = ketl_type_get_stack_size(it->variable.traits, it->variable.type);
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
				size = ketl_type_get_stack_size(it->variable.traits, it->variable.type);
				currentStackOffset -= size;
			}
			it = it->nextSibling;
		}
	}
}

ketl_ir_function_definition ketl_ir_builder_build(ketl_ir_builder* irBuilder, ketl_namespace* namespace, ketl_type_pointer returnType, ketl_syntax_node* syntaxNodeRoot, ketl_function_parameter* parameters, uint64_t parametersCount) {
	KETLIRFunctionWIP wip;

	wip.builder = irBuilder;
	wip.buildFailed = false;

	ketl_int_map_reset(&irBuilder->variablesMap);

	ketl_object_pool_reset(&irBuilder->scopedVariablesPool);
	ketl_object_pool_reset(&irBuilder->variablesPool);
	ketl_object_pool_reset(&irBuilder->operationsPool);
	ketl_object_pool_reset(&irBuilder->udelegatePool);
	ketl_object_pool_reset(&irBuilder->uvaluePool);
	ketl_object_pool_reset(&irBuilder->argumentPointersPool);
	ketl_object_pool_reset(&irBuilder->argumentsPool);

	ketl_int_map_reset(&irBuilder->operationReferMap);
	ketl_int_map_reset(&irBuilder->argumentsMap);

	wip.stack.stackRoot = NULL;
	wip.stack.currentStack = NULL;

	wip.returnOperations = NULL;
	wip.scopeIndex = 1;

	ketl_ir_variable** parameterArguments = NULL;
	if (parameters && parametersCount) {
		parameterArguments = malloc(sizeof(ketl_ir_variable*) * parametersCount);
		uint64_t currentStackOffset = 0;
		for (uint64_t i = 0u; i < parametersCount; ++i) {
			ketl_ir_variable parameter;

			parameter.traits = parameters[i].traits;
			parameter.type = parameters[i].type;

			uint64_t stackTypeSize = ketl_type_get_stack_size(parameter.traits, parameter.type);

			switch (stackTypeSize) {
			case 1:
				parameter.value.type = KETL_IR_ARGUMENT_TYPE_ARGUMENT_1_BYTE;
				break;
			case 2:
				parameter.value.type = KETL_IR_ARGUMENT_TYPE_ARGUMENT_2_BYTES;
				break;
			case 4:
				parameter.value.type = KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES;
				break;
			case 8:
				parameter.value.type = KETL_IR_ARGUMENT_TYPE_ARGUMENT_8_BYTES;
				break;
			KETL_NODEFAULT()
				KETL_DEBUGBREAK();
			}
			parameter.value.stack = currentStackOffset;

			currentStackOffset += stackTypeSize;

			const char* name = ketl_atomic_strings_get(&irBuilder->state->strings, parameters[i].name, KETL_NULL_TERMINATED_LENGTH);

			uint64_t scopeIndex = 0;

			ketl_ir_undefined_value** pCurrent;
			if (ketl_int_map_get_or_create(&irBuilder->variablesMap, (ketl_int_map_key)name, &pCurrent)) {
				*pCurrent = NULL;
			}
			else {
				// TODO check if the type is function, then we can overload
				if ((*pCurrent)->scopeIndex == scopeIndex) {
					KETL_ERROR("Parameter '%s' was already defined", name);
					wip.buildFailed = true;
					continue;
				}
			}

			ketl_ir_variable* pParameter = ketl_object_pool_get(&irBuilder->variablesPool);
			*pParameter = parameter;
			parameterArguments[i] = pParameter;

			ketl_ir_undefined_value* uvalue = wrapInUValueVariable(irBuilder, pParameter);
			uvalue->scopeIndex = scopeIndex;
			uvalue->next = *pCurrent;
			*pCurrent = uvalue;
		}
	}

	ketl_ir_operation* rootOperation = createOperationImpl(irBuilder);
	ketl_ir_operation_range range;
	range.root = rootOperation;
	range.next = createOperationImpl(irBuilder);
	buildIRCommand(&wip, &range, syntaxNodeRoot);

	if (range.root != NULL) {
		ketl_ir_operation* returnOperation = createLastOperationFromRange(&range);
		returnOperation->code = KETL_IR_CODE_RETURN;
		returnOperation->argumentCount = 0;

		ketl_ir_return_node* returnNode = ketl_object_pool_get(&irBuilder->returnPool);
		returnNode->next = wip.returnOperations;
		returnNode->operation = returnOperation;
		returnNode->returnVariable = NULL;
		returnNode->tempVariable = NULL;
		wip.returnOperations = returnNode;
	}
	// range.next would be not used, 
	// but we don't need to recycle it, 
	// since the building cycle came to the end

	ketl_ir_function_definition functionDefinition;
	functionDefinition.function = NULL;
	functionDefinition.type.base = NULL;

	if (returnType.base == NULL) {
		bool isProperReturnVoid = true;
		bool isProperReturnValue = true;

		for (ketl_ir_return_node* returnIt = wip.returnOperations; returnIt; returnIt = returnIt->next) {
			ketl_ir_operation* returnOperation = returnIt->operation;

			isProperReturnVoid &= returnOperation->code == KETL_IR_CODE_RETURN;
			isProperReturnValue &= returnOperation->code != KETL_IR_CODE_RETURN;
		}

		if (isProperReturnVoid && !isProperReturnValue) {
			returnType.primitive = &irBuilder->state->primitives.void_t;
		}
		else if (!isProperReturnVoid && isProperReturnValue) {
			ketl_object_pool_reset(&irBuilder->castingPool);
			ketl_object_pool_reset(&irBuilder->castingReturnPool);

			ReturnCastingOption* returnSet = NULL;

			ketl_ir_return_node* returnIt = wip.returnOperations;

			{
				CastingOption* possibleCastingSet = possibleCastingForDelegate(irBuilder, returnIt->returnVariable);

				for (; possibleCastingSet; possibleCastingSet = possibleCastingSet->next) {
					ReturnCastingOption* returnCasting = ketl_object_pool_get(&irBuilder->castingReturnPool);

					returnCasting->score = possibleCastingSet->score;
					if (possibleCastingSet->operator) {
						ketl_cast_operator* castOperator = possibleCastingSet->operator;
						returnCasting->type = castOperator->outputType;
						returnCasting->traits = castOperator->outputTraits;
					}
					else {
						ketl_ir_variable* variable = possibleCastingSet->variable;
						returnCasting->type = variable->type;
						returnCasting->traits = variable->traits;
					}
					returnCasting->next = returnSet;
					returnSet = returnCasting;
				}
			}

			returnIt = returnIt->next;
			for (; returnIt; returnIt = returnIt->next) {
				ReturnCastingOption* returnSetOld = returnSet;
				returnSet = NULL;

				CastingOption* possibleCasting = possibleCastingForDelegate(irBuilder, returnIt->returnVariable);
				for (; possibleCasting; possibleCasting = possibleCasting->next) {
					ketl_type_pointer castingType;
					//ketl_variable_traits castingTraits;
					if (possibleCasting->operator) {
						ketl_cast_operator* castOperator = possibleCasting->operator;
						castingType = castOperator->outputType;
						//castingTraits = castOperator->outputTraits;
					}
					else {
						ketl_ir_variable* variable = possibleCasting->variable;
						castingType = variable->type;
						//castingTraits = variable->traits;
					}

					ReturnCastingOption* returnOldPrev = NULL;
					ReturnCastingOption* returnOldIt = returnSetOld;
					while (returnOldIt) {
						if (castingType.base == returnOldIt->type.base/* &&
							castingTraits.hash == returnOldIt->traits.hash*/) {
							
							if (returnOldPrev) {
								returnOldPrev->next = returnOldIt->next;
							}

							returnOldIt->score += possibleCasting->score;
							returnOldIt->next = returnSet;
							returnSet = returnOldIt;

							break;
						}

						returnOldPrev = returnOldIt;
						returnOldIt = returnOldIt->next;
					}
				}
			}

			ketl_type_pointer winningType = { NULL };
			uint64_t bestScore = UINT64_MAX;

			for (; returnSet; returnSet = returnSet->next) {
				if (returnSet->score < bestScore) {
					winningType = returnSet->type;
					bestScore = returnSet->score;
				}
				// TODO removed as temporary fix
				/*
				else if (returnSet->score == bestScore) {
					winningType.base = NULL;
				}
				*/
			}
			
			// TODO check for NULL type;
			if (KETL_CHECK_VOEM(winningType.base != NULL, "Can't deduce return type", 0)) {
				return functionDefinition;
			}
			returnType = winningType;
		}
		else {
			KETL_ERROR("Not all return statements return value", 0);
			// TODO proper log error
		}
	}

	// do last conversions
	if (returnType.primitive != &irBuilder->state->primitives.void_t) {
		ketl_scoped_variable* savedTempVarRoot = wip.vars.tempVariables;
		
		for (ketl_ir_return_node* returnIt = wip.returnOperations; returnIt; returnIt = returnIt->next) {
			wip.vars.tempVariables = returnIt->tempVariable;

			ketl_object_pool_reset(&irBuilder->castingPool);
			CastingOption* expressionCasting = castDelegateToVariable(irBuilder, returnIt->returnVariable, returnType);

			if (expressionCasting == NULL) {
				KETL_ERROR("Can't cast expression to type '%s'", 
					returnType.base->name);
				wip.buildFailed = true;
				continue;
			}

			ketl_ir_operation_range innerRange;
			innerRange.root = returnIt->operation;
			innerRange.next = createOperationImpl(irBuilder);
			convertValues(&wip, &innerRange, expressionCasting);

			ketl_ir_variable* expressionVarible = expressionCasting->variable;

			ketl_ir_operation* operation = createLastOperationFromRange(&innerRange);

			switch (ketl_type_get_stack_size(expressionVarible->traits, expressionVarible->type)) {
			case 1:
				operation->code = KETL_IR_CODE_RETURN_1_BYTE;
				break;
			case 2:
				operation->code = KETL_IR_CODE_RETURN_2_BYTES;
				break;
			case 4:
				operation->code = KETL_IR_CODE_RETURN_4_BYTES;
				break;
			case 8:
				operation->code = KETL_IR_CODE_RETURN_8_BYTES;
				break;
			default:
				KETL_DEBUGBREAK();
			}
			operation->argumentCount = 1;
			operation->arguments = ketl_object_pool_get_array(&irBuilder->argumentPointersPool, 1);
			operation->arguments[0] = &expressionVarible->value;
		}
		wip.vars.tempVariables = savedTempVarRoot;
	}

	if (wip.buildFailed) {
		return functionDefinition;
	}

	// we are golden, lets build it

	// if namespace is set, this is the time to fill it
	if (namespace) {
		ketl_int_map_iterator childrenIterator;
		ketl_int_map_iterator_init(&childrenIterator, &irBuilder->variablesMap);
		for (; ketl_int_map_iterator_has_next(&childrenIterator); ketl_int_map_iterator_next(&childrenIterator)) {
			const char* name;
			ketl_ir_undefined_value** uvalue;
			ketl_int_map_iterator_get(&childrenIterator, (ketl_int_map_key*)(&name), &uvalue);

			if ((*uvalue)->scopeIndex != 1) {
				continue;
			}

			ketl_scoped_variable* pStackVariable = (ketl_scoped_variable*)(*uvalue)->variable;
			ketl_scoped_variable stackVariable = *pStackVariable;

			ketl_scoped_variable* replacement = stackVariable.firstChild;
			if (replacement) {
				ketl_scoped_variable* children = replacement;
				while (children) {
					children->parent = stackVariable.parent;
					children = children->nextSibling;
				}
			} else {
				replacement = stackVariable.nextSibling;
			}

			ketl_scoped_variable* firstSibling;
			if (stackVariable.parent) {
				firstSibling = stackVariable.parent->firstChild;
			} else {
				firstSibling = wip.stack.stackRoot;
			}

			if (firstSibling != pStackVariable) {
				while(firstSibling->nextSibling != pStackVariable) {
					firstSibling = firstSibling->nextSibling;
				}
				firstSibling->nextSibling = replacement;
			} else {
				if (wip.stack.stackRoot == pStackVariable) {
					wip.stack.stackRoot = replacement;
				} else {
					stackVariable.parent->firstChild = replacement;
				}
			}

			stackVariable.variable.value.pointer = ketl_state_define_internal_variable(wip.builder->state, name, stackVariable.variable.type);
			stackVariable.variable.value.type = KETL_IR_ARGUMENT_TYPE_POINTER;
			pStackVariable->variable.value = stackVariable.variable.value;
		}
	}

	ketl_int_map_reset(&irBuilder->operationReferMap);
	ketl_int_map_reset(&irBuilder->argumentsMap);
	ketl_stack_reset(&irBuilder->extraNextStack);

	if (parameterArguments) {
		for (uint64_t i = 0u; i < parametersCount; ++i) {
			uint64_t* newIndex;
			ketl_int_map_get_or_create(&irBuilder->argumentsMap, (ketl_int_map_key)&parameterArguments[i]->value, &newIndex);
			*newIndex = i;
		}
		free(parameterArguments);
	}

	*(ketl_ir_operation**)ketl_stack_push(&irBuilder->extraNextStack) = rootOperation;

	while (!ketl_stack_is_empty(&irBuilder->extraNextStack)) {
		ketl_ir_operation* itOperation = *(ketl_ir_operation**)ketl_stack_peek(&irBuilder->extraNextStack);
		ketl_stack_pop(&irBuilder->extraNextStack);
		countOperationsAndArguments(itOperation, irBuilder);
	}

	uint64_t operationCount = ketl_int_map_get_size(&irBuilder->operationReferMap);
	uint64_t argumentsCount = ketl_int_map_get_size(&irBuilder->argumentsMap);

	uint64_t maxStackOffset = bakeStackUsage(wip.stack.stackRoot);

	// TODO align
	uint64_t totalSize = sizeof(ketl_ir_function) + sizeof(ketl_ir_argument) * argumentsCount + sizeof(ketl_ir_operation) * operationCount;
	uint8_t* rawPointer = malloc(totalSize);

 	functionDefinition.function = (ketl_ir_function*)rawPointer;
	rawPointer += sizeof(ketl_ir_function);

	functionDefinition.function->stackUsage = maxStackOffset;

	functionDefinition.function->arguments = (ketl_ir_argument*)rawPointer;
	rawPointer += sizeof(ketl_ir_argument) * argumentsCount;

	ketl_int_map_iterator argumentIterator;
	ketl_int_map_iterator_init(&argumentIterator, &irBuilder->argumentsMap);
	while (ketl_int_map_iterator_has_next(&argumentIterator)) {
		ketl_ir_argument* oldArgument;
		uint64_t* argumentNewIndex;
		ketl_int_map_iterator_get(&argumentIterator, (ketl_int_map_key*)&oldArgument, &argumentNewIndex);
		functionDefinition.function->arguments[*argumentNewIndex] = *oldArgument;
		ketl_int_map_iterator_next(&argumentIterator);
	}

	functionDefinition.function->operationsCount = operationCount;
	functionDefinition.function->operations = (ketl_ir_operation*)rawPointer;

	ketl_int_map_iterator operationIterator;
	ketl_int_map_iterator_init(&operationIterator, &irBuilder->operationReferMap);
	while (ketl_int_map_iterator_has_next(&operationIterator)) {
		ketl_ir_operation* pOldOperation;
		uint64_t* operationNewIndex;
		ketl_int_map_iterator_get(&operationIterator, (ketl_int_map_key*)&pOldOperation, &operationNewIndex);
		ketl_ir_operation oldOperation = *pOldOperation;
		ketl_ir_argument* arguments = functionDefinition.function->arguments;
		for (uint64_t i = 0; i < oldOperation.argumentCount; ++i) {
			uint64_t* argumentNewIndex = ketl_int_map_get(&irBuilder->argumentsMap, (ketl_int_map_key)oldOperation.arguments[i]);
			oldOperation.arguments[i] = &arguments[*argumentNewIndex];
		}
		if (oldOperation.mainNext != NULL) {
			uint64_t* operationNewNextIndex = ketl_int_map_get(&irBuilder->operationReferMap, (ketl_int_map_key)oldOperation.mainNext);
			oldOperation.mainNext = &functionDefinition.function->operations[*operationNewNextIndex];
		}
		if (oldOperation.extraNext != NULL) {
			uint64_t* operationNewNextIndex = ketl_int_map_get(&irBuilder->operationReferMap, (ketl_int_map_key)oldOperation.extraNext);
			oldOperation.extraNext = &functionDefinition.function->operations[*operationNewNextIndex];
		}
		functionDefinition.function->operations[*operationNewIndex] = oldOperation;
		ketl_int_map_iterator_next(&operationIterator);
	}

	++parametersCount;
	ketl_type_parameters* typeParameters = malloc(sizeof(ketl_type_parameters) * parametersCount);
	typeParameters[0].type = returnType;
	typeParameters[0].traits.values.isNullable = false;
	typeParameters[0].traits.values.isConst = false;
	typeParameters[0].traits.values.type = KETL_TRAIT_TYPE_RVALUE;
	for (uint64_t i = 1u; i < parametersCount; ++i) {
		typeParameters[i].type = parameters[i - 1].type;
		typeParameters[i].traits = parameters[i - 1].traits;
	}
	functionDefinition.type = getFunctionType(irBuilder->state, typeParameters, parametersCount);
	free(typeParameters);

	return functionDefinition;
}