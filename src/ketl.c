//🍲ketl
#include "ketl/ketl.h"

#include "compiler/ir_builder.h"

#include "ketl/compiler/syntax_node.h"
#include "ketl/type.h"

#include <string.h>

KETL_DEFINE(TypeFunctionSearchNodeByTraits) {
	KETLTypeFunction* leaf;
	KETLIntMap children;
	bool initialized;
};

KETL_DEFINE(TypeFunctionSearchNode) {
	TypeFunctionSearchNodeByTraits byTraits[KETL_VARIABLE_TRAIT_HASH_COUNT];
};

static void initTypeFunctionSearchNode(TypeFunctionSearchNode* node) {
	for (int8_t i = 0u; i < KETL_VARIABLE_TRAIT_HASH_COUNT; ++i) {
		node->byTraits[i].initialized = false;
		node->byTraits[i].leaf = NULL;

	}
}

static void deinitTypeFunctionSearchNode(TypeFunctionSearchNode* node) {
	for (int8_t i = 0u; i < KETL_VARIABLE_TRAIT_HASH_COUNT; ++i) {
		TypeFunctionSearchNodeByTraits* byTraits = &node->byTraits[i];
		if (!byTraits->initialized) {
			return;
		}

		KETLIntMapIterator childrenIterator;
		ketlInitIntMapIterator(&childrenIterator, &byTraits->children);
		while (ketlIntMapIteratorHasNext(&childrenIterator)) {
			KETLTypePtr type;
			TypeFunctionSearchNode* child;
			ketlIntMapIteratorGet(&childrenIterator, (KETLIntMapKey*)(&type.base), &child);

			deinitTypeFunctionSearchNode(child);
			ketlIntMapIteratorNext(&childrenIterator);
		}
	}
}

static KETLTypeFunction* getTypeFunction(KETLState* state, KETLIntMap* typeFunctionMap, KETLTypeParameters* parameters, uint64_t lastParameter, uint64_t currentParameter) {
	TypeFunctionSearchNode* child;
	if (ketlIntMapGetOrCreate(typeFunctionMap, (KETLIntMapKey)parameters[currentParameter].type.base, &child)) {
		initTypeFunctionSearchNode(child);
	}

	TypeFunctionSearchNodeByTraits* byTraits = &child->byTraits[parameters[currentParameter].traits.hash];
	if (currentParameter == lastParameter) {
		if (byTraits->leaf != NULL) {
			return byTraits->leaf;
		}

		KETLTypeFunction* function = ketlGetFreeObjectFromPool(&state->typeFunctionsPool);
		function->kind = KETL_TYPE_KIND_FUNCTION;
		function->name = NULL; // TODO construct name
		function->parameters = ketlGetNFreeObjectsFromPool(&state->typeParametersPool, function->parametersCount = (uint32_t)(lastParameter + 1));
		memcpy(function->parameters, parameters, sizeof(KETLTypeParameters) * function->parametersCount);
		uint64_t offset = 0;
		for (uint64_t i = 1u; i <= lastParameter; ++i) {
			function->parameters[i].offset = offset;
			offset += getStackTypeSize(function->parameters[i].traits, function->parameters[i].type);
		}
		return function;
	}
	else {
		if (!byTraits->initialized) {
			byTraits->initialized = true;
			ketlInitIntMap(&byTraits->children, sizeof(TypeFunctionSearchNode), 16);
		}
		return getTypeFunction(state, &byTraits->children, parameters, lastParameter, currentParameter + 1);
	}
}

static void initNamespace(KETLNamespace* namespace) {
	ketlInitIntMap(&namespace->types, sizeof(KETLTypePtr), 16);
	ketlInitIntMap(&namespace->variables, sizeof(IRUndefinedValue*), 16);
	ketlInitIntMap(&namespace->namespaces, sizeof(KETLNamespace), 8);
}

static void deinitNamespace(KETLNamespace* namespace) {
	ketlDeinitIntMap(&namespace->namespaces);
	ketlDeinitIntMap(&namespace->variables);
	ketlDeinitIntMap(&namespace->types);
}

static void createPrimitive(KETLTypePrimitive* type, const char* name, uint64_t size, KETLNamespace* globalNamespace) {
	type->name = name;
	type->kind = KETL_TYPE_KIND_PRIMITIVE;
	type->size = (uint32_t)size;

	KETLTypePtr* typePtr;
	ketlIntMapGetOrCreate(&globalNamespace->types, (KETLIntMapKey)name, &typePtr);
	typePtr->primitive = type;
}

static void registerPrimitiveBinaryOperator(KETLState* state, KETLOperatorCode operatorCode, KETLIROperationCode operationCode, KETLTypePrimitive* type) {
	KETLBinaryOperator** pOperator;
	if (ketlIntMapGetOrCreate(&state->binaryOperators, operatorCode, &pOperator)) {
		*pOperator = NULL;
	}

	KETLBinaryOperator operator;
	KETLVariableTraits traits;

	traits.type = KETL_TRAIT_TYPE_REF_IN;
	traits.isNullable = false;
	traits.isConst = true;

	operator.code = operationCode;

	operator.lhsTraits = traits;
	operator.rhsTraits = traits;
	operator.outputTraits = traits;
	operator.outputTraits.type = KETL_TRAIT_TYPE_RVALUE;

	operator.lhsType.primitive = type;
	operator.rhsType.primitive = type;
	operator.outputType.primitive = type;

	operator.next = *pOperator;
	KETLBinaryOperator* newOperator = ketlGetFreeObjectFromPool(&state->binaryOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;
}

static void registerPrimitiveComparisonOperator(KETLState* state, KETLOperatorCode operatorCode, KETLIROperationCode operationCode, KETLTypePrimitive* type) {
	KETLBinaryOperator** pOperator;
	if (ketlIntMapGetOrCreate(&state->binaryOperators, operatorCode, &pOperator)) {
		*pOperator = NULL;
	}

	KETLBinaryOperator operator;
	KETLVariableTraits traits;

	traits.type = KETL_TRAIT_TYPE_REF_IN;
	traits.isNullable = false;
	traits.isConst = true;

	operator.code = operationCode;

	operator.lhsTraits = traits;
	operator.rhsTraits = traits;
	operator.outputTraits = traits;
	operator.outputTraits.type = KETL_TRAIT_TYPE_RVALUE;

	operator.lhsType.primitive = type;
	operator.rhsType.primitive = type;
	operator.outputType.primitive = &state->primitives.bool_t;

	operator.next = *pOperator;
	KETLBinaryOperator* newOperator = ketlGetFreeObjectFromPool(&state->binaryOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;
}

static void registerPrimitiveCastOperator(KETLState* state, KETLTypePrimitive* sourceType, KETLTypePrimitive* targetType, KETLIROperationCode operationCode, bool implicit) {
	KETLCastOperator** pOperator;
	if (ketlIntMapGetOrCreate(&state->castOperators, (KETLIntMapKey)sourceType, &pOperator)) {
		*pOperator = NULL;
	}

	KETLCastOperator operator;
	KETLVariableTraits traits;

	traits.type = KETL_TRAIT_TYPE_REF_IN;
	traits.isNullable = false;
	traits.isConst = true;

	operator.code = operationCode;

	operator.inputTraits = traits;
	operator.outputTraits = traits;
	operator.outputTraits.type = KETL_TRAIT_TYPE_RVALUE;

	operator.outputType.primitive = targetType;
	operator.implicit = implicit;

	operator.next = *pOperator;
	KETLCastOperator* newOperator = ketlGetFreeObjectFromPool(&state->castOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;

}

void ketlDeinitState(KETLState* state) {
	ketlDeinitIntMap(&state->typeFunctionSearchMap);
	ketlDeinitObjectPool(&state->typeFunctionsPool);
	ketlDeinitObjectPool(&state->typeParametersPool);

	ketlDeinitObjectPool(&state->variablesPool);
	ketlDeinitObjectPool(&state->undefVarPool);

	ketlDeinitIntMap(&state->castOperators);
	ketlDeinitIntMap(&state->binaryOperators);
	ketlDeinitIntMap(&state->unaryOperators);
	ketlDeinitObjectPool(&state->castOperatorsPool);
	ketlDeinitObjectPool(&state->binaryOperatorsPool);
	ketlDeinitObjectPool(&state->unaryOperatorsPool);

	deinitNamespace(&state->globalNamespace);
	ketlIRCompilerDeinit(&state->irCompiler);
	ketlDeinitCompiler(&state->compiler);
	ketlDeinitAtomicStrings(&state->strings);
}

void ketlInitState(KETLState* state) {
	ketlInitAtomicStrings(&state->strings, 16);
	ketlInitCompiler(&state->compiler, state);
	ketlIRCompilerInit(&state->irCompiler);
	initNamespace(&state->globalNamespace);

	ketlInitObjectPool(&state->unaryOperatorsPool, sizeof(KETLUnaryOperator), 8);
	ketlInitObjectPool(&state->binaryOperatorsPool, sizeof(KETLBinaryOperator), 8);
	ketlInitObjectPool(&state->castOperatorsPool, sizeof(KETLCastOperator), 8);
	ketlInitIntMap(&state->unaryOperators, sizeof(KETLUnaryOperator*), 4);
	ketlInitIntMap(&state->binaryOperators, sizeof(KETLBinaryOperator*), 4);
	ketlInitIntMap(&state->castOperators, sizeof(KETLCastOperator*), 4);

	ketlInitObjectPool(&state->undefVarPool, sizeof(IRUndefinedValue), 16);
	ketlInitObjectPool(&state->variablesPool, sizeof(KETLIRVariable), 16);

	ketlInitObjectPool(&state->typeParametersPool, sizeof(KETLTypeParameters), 16);
	ketlInitObjectPool(&state->typeFunctionsPool, sizeof(KETLTypeFunction), 16);
	ketlInitIntMap(&state->typeFunctionSearchMap, sizeof(TypeFunctionSearchNode), 16);

	createPrimitive(&state->primitives.void_t, ketlAtomicStringsGet(&state->strings, "void", KETL_NULL_TERMINATED_LENGTH), 0, &state->globalNamespace);
	createPrimitive(&state->primitives.bool_t, ketlAtomicStringsGet(&state->strings, "bool", KETL_NULL_TERMINATED_LENGTH), sizeof(bool), &state->globalNamespace);
	createPrimitive(&state->primitives.i8_t, ketlAtomicStringsGet(&state->strings, "i8", KETL_NULL_TERMINATED_LENGTH), sizeof(int8_t), &state->globalNamespace);
	createPrimitive(&state->primitives.i16_t, ketlAtomicStringsGet(&state->strings, "i16", KETL_NULL_TERMINATED_LENGTH), sizeof(int16_t), &state->globalNamespace);
	createPrimitive(&state->primitives.i32_t, ketlAtomicStringsGet(&state->strings, "i32", KETL_NULL_TERMINATED_LENGTH), sizeof(int32_t), &state->globalNamespace);
	createPrimitive(&state->primitives.i64_t, ketlAtomicStringsGet(&state->strings, "i64", KETL_NULL_TERMINATED_LENGTH), sizeof(int64_t), &state->globalNamespace);
	createPrimitive(&state->primitives.f32_t, ketlAtomicStringsGet(&state->strings, "f32", KETL_NULL_TERMINATED_LENGTH), sizeof(float), &state->globalNamespace);
	createPrimitive(&state->primitives.f64_t, ketlAtomicStringsGet(&state->strings, "f64", KETL_NULL_TERMINATED_LENGTH), sizeof(double), &state->globalNamespace);

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PLUS, KETL_IR_CODE_ADD_INT8, &state->primitives.i8_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PLUS, KETL_IR_CODE_ADD_INT16, &state->primitives.i16_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PLUS, KETL_IR_CODE_ADD_INT32, &state->primitives.i32_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PLUS, KETL_IR_CODE_ADD_INT64, &state->primitives.i64_t);

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_MINUS, KETL_IR_CODE_SUB_INT8, &state->primitives.i8_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_MINUS, KETL_IR_CODE_SUB_INT16, &state->primitives.i16_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_MINUS, KETL_IR_CODE_SUB_INT32, &state->primitives.i32_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_MINUS, KETL_IR_CODE_SUB_INT64, &state->primitives.i64_t);

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PROD, KETL_IR_CODE_MULTY_INT8, &state->primitives.i8_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PROD, KETL_IR_CODE_MULTY_INT16, &state->primitives.i16_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PROD, KETL_IR_CODE_MULTY_INT32, &state->primitives.i32_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PROD, KETL_IR_CODE_MULTY_INT64, &state->primitives.i64_t);

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_DIV, KETL_IR_CODE_DIV_INT8, &state->primitives.i8_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_DIV, KETL_IR_CODE_DIV_INT16, &state->primitives.i16_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_DIV, KETL_IR_CODE_DIV_INT32, &state->primitives.i32_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_DIV, KETL_IR_CODE_DIV_INT64, &state->primitives.i64_t);

	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_EQUAL, KETL_IR_CODE_EQUAL_INT8, &state->primitives.i8_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_EQUAL, KETL_IR_CODE_EQUAL_INT16, &state->primitives.i16_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_EQUAL, KETL_IR_CODE_EQUAL_INT32, &state->primitives.i32_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_EQUAL, KETL_IR_CODE_EQUAL_INT64, &state->primitives.i64_t);

	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_UNEQUAL, KETL_IR_CODE_UNEQUAL_INT8, &state->primitives.i8_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_UNEQUAL, KETL_IR_CODE_UNEQUAL_INT16, &state->primitives.i16_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_UNEQUAL, KETL_IR_CODE_UNEQUAL_INT32, &state->primitives.i32_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_UNEQUAL, KETL_IR_CODE_UNEQUAL_INT64, &state->primitives.i64_t);

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_ASSIGN, KETL_IR_CODE_ASSIGN_8_BYTES, &state->primitives.i64_t);

	registerPrimitiveCastOperator(state, &state->primitives.i8_t, &state->primitives.i64_t, KETL_IR_CODE_CAST_INT8_INT64, true);
	registerPrimitiveCastOperator(state, &state->primitives.i8_t, &state->primitives.i32_t, KETL_IR_CODE_CAST_INT8_INT32, true);
	registerPrimitiveCastOperator(state, &state->primitives.i8_t, &state->primitives.i16_t, KETL_IR_CODE_CAST_INT8_INT16, true);
	registerPrimitiveCastOperator(state, &state->primitives.i8_t, &state->primitives.f64_t, KETL_IR_CODE_CAST_INT8_FLOAT64, true);
	registerPrimitiveCastOperator(state, &state->primitives.i8_t, &state->primitives.f32_t, KETL_IR_CODE_CAST_INT8_FLOAT32, true);

	registerPrimitiveCastOperator(state, &state->primitives.i16_t, &state->primitives.i64_t, KETL_IR_CODE_CAST_INT16_INT64, true);
	registerPrimitiveCastOperator(state, &state->primitives.i16_t, &state->primitives.i32_t, KETL_IR_CODE_CAST_INT16_INT32, true);
	registerPrimitiveCastOperator(state, &state->primitives.i16_t, &state->primitives.i8_t, KETL_IR_CODE_CAST_INT16_INT8, false);
	registerPrimitiveCastOperator(state, &state->primitives.i16_t, &state->primitives.f64_t, KETL_IR_CODE_CAST_INT16_FLOAT64, true);
	registerPrimitiveCastOperator(state, &state->primitives.i16_t, &state->primitives.f32_t, KETL_IR_CODE_CAST_INT16_FLOAT32, true);

	registerPrimitiveCastOperator(state, &state->primitives.i32_t, &state->primitives.i64_t, KETL_IR_CODE_CAST_INT32_INT64, true);
	registerPrimitiveCastOperator(state, &state->primitives.i32_t, &state->primitives.i16_t, KETL_IR_CODE_CAST_INT32_INT16, false);
	registerPrimitiveCastOperator(state, &state->primitives.i32_t, &state->primitives.i8_t, KETL_IR_CODE_CAST_INT32_INT8, false);
	registerPrimitiveCastOperator(state, &state->primitives.i32_t, &state->primitives.f64_t, KETL_IR_CODE_CAST_INT32_FLOAT64, true);
	registerPrimitiveCastOperator(state, &state->primitives.i32_t, &state->primitives.f32_t, KETL_IR_CODE_CAST_INT32_FLOAT32, false);

	registerPrimitiveCastOperator(state, &state->primitives.i64_t, &state->primitives.i32_t, KETL_IR_CODE_CAST_INT64_INT32, false);
	registerPrimitiveCastOperator(state, &state->primitives.i64_t, &state->primitives.i16_t, KETL_IR_CODE_CAST_INT64_INT16, false);
	registerPrimitiveCastOperator(state, &state->primitives.i64_t, &state->primitives.i8_t, KETL_IR_CODE_CAST_INT64_INT8, false);
	registerPrimitiveCastOperator(state, &state->primitives.i64_t, &state->primitives.f64_t, KETL_IR_CODE_CAST_INT64_FLOAT64, false);
	registerPrimitiveCastOperator(state, &state->primitives.i64_t, &state->primitives.f32_t, KETL_IR_CODE_CAST_INT64_FLOAT32, false);

	registerPrimitiveCastOperator(state, &state->primitives.f32_t, &state->primitives.f64_t, KETL_IR_CODE_CAST_FLOAT32_FLOAT64, true);
	registerPrimitiveCastOperator(state, &state->primitives.f32_t, &state->primitives.i64_t, KETL_IR_CODE_CAST_FLOAT32_INT64, false);
	registerPrimitiveCastOperator(state, &state->primitives.f32_t, &state->primitives.i32_t, KETL_IR_CODE_CAST_FLOAT32_INT32, false);
	registerPrimitiveCastOperator(state, &state->primitives.f32_t, &state->primitives.i16_t, KETL_IR_CODE_CAST_FLOAT32_INT16, false);
	registerPrimitiveCastOperator(state, &state->primitives.f32_t, &state->primitives.i8_t, KETL_IR_CODE_CAST_FLOAT32_INT8, false);

	registerPrimitiveCastOperator(state, &state->primitives.f64_t, &state->primitives.f32_t, KETL_IR_CODE_CAST_FLOAT64_FLOAT32, false);
	registerPrimitiveCastOperator(state, &state->primitives.f64_t, &state->primitives.i64_t, KETL_IR_CODE_CAST_FLOAT64_INT64, false);
	registerPrimitiveCastOperator(state, &state->primitives.f64_t, &state->primitives.i32_t, KETL_IR_CODE_CAST_FLOAT64_INT32, false);
	registerPrimitiveCastOperator(state, &state->primitives.f64_t, &state->primitives.i16_t, KETL_IR_CODE_CAST_FLOAT64_INT16, false);
	registerPrimitiveCastOperator(state, &state->primitives.f64_t, &state->primitives.i8_t, KETL_IR_CODE_CAST_FLOAT64_INT8, false);
}

#include <stdlib.h>

void ketlDefineVariable(KETLState* state, const char* name, KETLTypePtr type, void* pointer) {
	KETLIRVariable* variable = ketlGetFreeObjectFromPool(&state->variablesPool);
	variable->type = type;
	variable->traits.isConst = false;
	variable->traits.isNullable = false;
	variable->traits.type = KETL_TRAIT_TYPE_LVALUE;
	variable->value.type = KETL_IR_ARGUMENT_TYPE_POINTER;
	variable->value.pointer = pointer;

	IRUndefinedValue* uvalue = ketlGetFreeObjectFromPool(&state->undefVarPool);
	uvalue->variable = variable;
	uvalue->scopeIndex = 0;
	uvalue->next = NULL;

	const char* uniqName = ketlAtomicStringsGet(&state->strings, name, KETL_NULL_TERMINATED_LENGTH);

	IRUndefinedValue** ppUValue;
	ketlIntMapGetOrCreate(&state->globalNamespace.variables, (KETLIntMapKey)uniqName, &ppUValue);
	*ppUValue = uvalue;
}

KETLFunction* ketlCompileFunction(KETLState* state, const char* source, KETLParameter* parameters, uint64_t parametersCount) {
	KETLSyntaxNode* root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &state->compiler.bytecodeCompiler.syntaxSolver, &state->compiler.bytecodeCompiler.syntaxNodePool);
	if (!root) {
		return NULL;
	}

	KETLIRFunctionDefinition irFunction = ketlBuildIR((KETLTypePtr){ NULL }, &state->compiler.irBuilder, root, parameters, parametersCount);
	if (irFunction.function == NULL) {
		return NULL;
	}

	// TODO optimization on ir

	KETLFunction* function = ketlCompileIR(&state->irCompiler, irFunction.function);
	free(irFunction.function);
	return function;
}

KETLTypePtr getFunctionType(KETLState* state, KETLTypeParameters* parameters, uint64_t parametersCount) {
	KETLTypePtr result;
	result.function = getTypeFunction(state, &state->typeFunctionSearchMap, parameters, parametersCount - 1, 0);
	return result;
}

