﻿//🍲ketl
#include "ketl/ketl.h"

#include "compiler/ir_builder.h"

#include "ketl/compiler/syntax_node.h"
#include "ketl/type.h"

static void initNamespace(KETLNamespace* namespace) {
	ketlInitIntMap(&namespace->types, sizeof(KETLType), 16);
	ketlInitIntMap(&namespace->variables, sizeof(IRUndefinedValue*), 16);
	ketlInitIntMap(&namespace->namespaces, sizeof(KETLNamespace), 8);
}

static void deinitNamespace(KETLNamespace* namespace) {
	ketlDeinitIntMap(&namespace->namespaces);
	ketlDeinitIntMap(&namespace->variables);
	ketlDeinitIntMap(&namespace->types);
}

static KETLType* createPrimitive(KETLState* state, const char* name, uint64_t size) {
	name = ketlAtomicStringsGet(&state->strings, name, KETL_NULL_TERMINATED_LENGTH);
	
	KETLType* type;
	ketlIntMapGetOrCreate(&state->globalNamespace.types, (KETLIntMapKey)name, &type);

	type->name = name;
	type->kind = KETL_TYPE_KIND_PRIMITIVE;
	type->size = size;
	// TODO
	//ketlInitIntMap(&type->variables, sizeof(KETLTypeVariable), 0);

	return type;
}

static void registerPrimitiveBinaryOperator(KETLState* state, KETLOperatorCode operatorCode, KETLIROperationCode operationCode, KETLType* type) {
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

	operator.lhsType = type;
	operator.rhsType = type;
	operator.outputType = type;

	operator.next = *pOperator;
	KETLBinaryOperator* newOperator = ketlGetFreeObjectFromPool(&state->binaryOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;
}

static void registerPrimitiveComparisonOperator(KETLState* state, KETLOperatorCode operatorCode, KETLIROperationCode operationCode, KETLType* type) {
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

	operator.lhsType = type;
	operator.rhsType = type;
	operator.outputType = state->primitives.bool_t;

	operator.next = *pOperator;
	KETLBinaryOperator* newOperator = ketlGetFreeObjectFromPool(&state->binaryOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;
}

static void registerCastOperator(KETLState* state, KETLType* sourceType, KETLType* targetType, KETLIROperationCode operationCode, bool implicit) {
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

	operator.outputType = targetType;
	operator.implicit = implicit;

	operator.next = *pOperator;
	KETLCastOperator* newOperator = ketlGetFreeObjectFromPool(&state->castOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;

}

void ketlDeinitState(KETLState* state) {
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

	state->primitives.void_t = createPrimitive(state, "void", 0);
	state->primitives.bool_t = createPrimitive(state, "bool", sizeof(bool));
	state->primitives.i8_t = createPrimitive(state, "i8", sizeof(int8_t));
	state->primitives.i16_t = createPrimitive(state, "i16", sizeof(int16_t));
	state->primitives.i32_t = createPrimitive(state, "i32", sizeof(int32_t));
	state->primitives.i64_t = createPrimitive(state, "i64", sizeof(int64_t));
	state->primitives.f32_t = createPrimitive(state, "f32", sizeof(float));
	state->primitives.f64_t = createPrimitive(state, "f64", sizeof(double));

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PLUS, KETL_IR_CODE_ADD_INT8, state->primitives.i8_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PLUS, KETL_IR_CODE_ADD_INT16, state->primitives.i16_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PLUS, KETL_IR_CODE_ADD_INT32, state->primitives.i32_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PLUS, KETL_IR_CODE_ADD_INT64, state->primitives.i64_t);

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_MINUS, KETL_IR_CODE_SUB_INT8, state->primitives.i8_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_MINUS, KETL_IR_CODE_SUB_INT16, state->primitives.i16_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_MINUS, KETL_IR_CODE_SUB_INT32, state->primitives.i32_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_MINUS, KETL_IR_CODE_SUB_INT64, state->primitives.i64_t);

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PROD, KETL_IR_CODE_MULTY_INT8, state->primitives.i8_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PROD, KETL_IR_CODE_MULTY_INT16, state->primitives.i16_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PROD, KETL_IR_CODE_MULTY_INT32, state->primitives.i32_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_PROD, KETL_IR_CODE_MULTY_INT64, state->primitives.i64_t);

	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_DIV, KETL_IR_CODE_DIV_INT8, state->primitives.i8_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_DIV, KETL_IR_CODE_DIV_INT16, state->primitives.i16_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_DIV, KETL_IR_CODE_DIV_INT32, state->primitives.i32_t);
	registerPrimitiveBinaryOperator(state, KETL_OPERATOR_CODE_BI_DIV, KETL_IR_CODE_DIV_INT64, state->primitives.i64_t);

	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_EQUAL, KETL_IR_CODE_EQUAL_INT8, state->primitives.i8_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_EQUAL, KETL_IR_CODE_EQUAL_INT16, state->primitives.i16_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_EQUAL, KETL_IR_CODE_EQUAL_INT32, state->primitives.i32_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_EQUAL, KETL_IR_CODE_EQUAL_INT64, state->primitives.i64_t);

	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_UNEQUAL, KETL_IR_CODE_UNEQUAL_INT8, state->primitives.i8_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_UNEQUAL, KETL_IR_CODE_UNEQUAL_INT16, state->primitives.i16_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_UNEQUAL, KETL_IR_CODE_UNEQUAL_INT32, state->primitives.i32_t);
	registerPrimitiveComparisonOperator(state, KETL_OPERATOR_CODE_BI_UNEQUAL, KETL_IR_CODE_UNEQUAL_INT64, state->primitives.i64_t);

	registerCastOperator(state, state->primitives.i8_t, state->primitives.i64_t, KETL_IR_CODE_CAST_INT8_INT64, true);
	registerCastOperator(state, state->primitives.i8_t, state->primitives.i32_t, KETL_IR_CODE_CAST_INT8_INT32, true);
	registerCastOperator(state, state->primitives.i8_t, state->primitives.i16_t, KETL_IR_CODE_CAST_INT8_INT16, true);
	registerCastOperator(state, state->primitives.i8_t, state->primitives.f64_t, KETL_IR_CODE_CAST_INT8_FLOAT64, true);
	registerCastOperator(state, state->primitives.i8_t, state->primitives.f32_t, KETL_IR_CODE_CAST_INT8_FLOAT32, true);

	registerCastOperator(state, state->primitives.i16_t, state->primitives.i64_t, KETL_IR_CODE_CAST_INT16_INT64, true);
	registerCastOperator(state, state->primitives.i16_t, state->primitives.i32_t, KETL_IR_CODE_CAST_INT16_INT32, true);
	registerCastOperator(state, state->primitives.i16_t, state->primitives.i8_t, KETL_IR_CODE_CAST_INT16_INT8, false);
	registerCastOperator(state, state->primitives.i16_t, state->primitives.f64_t, KETL_IR_CODE_CAST_INT16_FLOAT64, true);
	registerCastOperator(state, state->primitives.i16_t, state->primitives.f32_t, KETL_IR_CODE_CAST_INT16_FLOAT32, true);

	registerCastOperator(state, state->primitives.i32_t, state->primitives.i64_t, KETL_IR_CODE_CAST_INT32_INT64, true);
	registerCastOperator(state, state->primitives.i32_t, state->primitives.i16_t, KETL_IR_CODE_CAST_INT32_INT16, false);
	registerCastOperator(state, state->primitives.i32_t, state->primitives.i8_t, KETL_IR_CODE_CAST_INT32_INT8, false);
	registerCastOperator(state, state->primitives.i32_t, state->primitives.f64_t, KETL_IR_CODE_CAST_INT32_FLOAT64, true);
	registerCastOperator(state, state->primitives.i32_t, state->primitives.f32_t, KETL_IR_CODE_CAST_INT32_FLOAT32, false);

	registerCastOperator(state, state->primitives.i64_t, state->primitives.i32_t, KETL_IR_CODE_CAST_INT64_INT32, false);
	registerCastOperator(state, state->primitives.i64_t, state->primitives.i16_t, KETL_IR_CODE_CAST_INT64_INT16, false);
	registerCastOperator(state, state->primitives.i64_t, state->primitives.i8_t, KETL_IR_CODE_CAST_INT64_INT8, false);
	registerCastOperator(state, state->primitives.i64_t, state->primitives.f64_t, KETL_IR_CODE_CAST_INT64_FLOAT64, false);
	registerCastOperator(state, state->primitives.i64_t, state->primitives.f32_t, KETL_IR_CODE_CAST_INT64_FLOAT32, false);

	registerCastOperator(state, state->primitives.f32_t, state->primitives.f64_t, KETL_IR_CODE_CAST_FLOAT32_FLOAT64, true);
	registerCastOperator(state, state->primitives.f32_t, state->primitives.i64_t, KETL_IR_CODE_CAST_FLOAT32_INT64, false);
	registerCastOperator(state, state->primitives.f32_t, state->primitives.i32_t, KETL_IR_CODE_CAST_FLOAT32_INT32, false);
	registerCastOperator(state, state->primitives.f32_t, state->primitives.i16_t, KETL_IR_CODE_CAST_FLOAT32_INT16, false);
	registerCastOperator(state, state->primitives.f32_t, state->primitives.i8_t, KETL_IR_CODE_CAST_FLOAT32_INT8, false);

	registerCastOperator(state, state->primitives.f64_t, state->primitives.f32_t, KETL_IR_CODE_CAST_FLOAT64_FLOAT32, false);
	registerCastOperator(state, state->primitives.f64_t, state->primitives.i64_t, KETL_IR_CODE_CAST_FLOAT64_INT64, false);
	registerCastOperator(state, state->primitives.f64_t, state->primitives.i32_t, KETL_IR_CODE_CAST_FLOAT64_INT32, false);
	registerCastOperator(state, state->primitives.f64_t, state->primitives.i16_t, KETL_IR_CODE_CAST_FLOAT64_INT16, false);
	registerCastOperator(state, state->primitives.f64_t, state->primitives.i8_t, KETL_IR_CODE_CAST_FLOAT64_INT8, false);
}

#include <stdlib.h>

void ketlDefineVariable(KETLState* state, const char* name, KETLType* type, void* pointer) {
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

KETLFunction* ketlCompileFunction(KETLState* state, const char* source) {
	KETLSyntaxNode* root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &state->compiler.bytecodeCompiler.syntaxSolver, &state->compiler.bytecodeCompiler.syntaxNodePool);

	KETLIRFunction* irFunction = ketlBuildIR(NULL, &state->compiler.irBuilder, root);

	// TODO optimization on ir

	KETLFunction* function = ketlCompileIR(&state->irCompiler, irFunction);
	free(irFunction);
	return function;
}

