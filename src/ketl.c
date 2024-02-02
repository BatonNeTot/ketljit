//🫖ketl
#include "ketl_impl.h"

#include "variable_impl.h"
#include "compiler/ir_builder.h"
#include "compiler/syntax_node.h"
#include "ketl/type.h"

#include <string.h>

KETL_DEFINE(TypeFunctionSearchNodeByTraits) {
	ketl_type_function* leaf;
	ketl_int_map children;
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

		ketl_int_map_iterator childrenIterator;
		ketl_int_map_iterator_init(&childrenIterator, &byTraits->children);
		while (ketl_int_map_iterator_has_next(&childrenIterator)) {
			ketl_type_pointer type;
			TypeFunctionSearchNode* child;
			ketl_int_map_iterator_get(&childrenIterator, (ketl_int_map_key*)(&type.base), &child);

			deinitTypeFunctionSearchNode(child);
			ketl_int_map_iterator_next(&childrenIterator);
		}
	}
}

static ketl_type_function* getTypeFunction(ketl_state* state, ketl_int_map* typeFunctionMap, ketl_type_parameters* parameters, uint64_t lastParameter, uint64_t currentParameter) {
	TypeFunctionSearchNode* child;
	if (ketl_int_map_get_or_create(typeFunctionMap, (ketl_int_map_key)parameters[currentParameter].type.base, &child)) {
		initTypeFunctionSearchNode(child);
	}

	TypeFunctionSearchNodeByTraits* byTraits = &child->byTraits[parameters[currentParameter].traits.hash];
	if (currentParameter == lastParameter) {
		if (byTraits->leaf != NULL) {
			return byTraits->leaf;
		}

		ketl_type_function* function = ketl_object_pool_get(&state->typeFunctionsPool);
		function->kind = KETL_TYPE_KIND_FUNCTION;
		function->name = NULL; // TODO construct name
		function->parameters = ketl_object_pool_get_array(&state->typeParametersPool, function->parameterCount = (uint32_t)(lastParameter + 1));
		memcpy(function->parameters, parameters, sizeof(ketl_type_parameters) * function->parameterCount);
		uint64_t offset = 0;
		for (uint64_t i = 1u; i <= lastParameter; ++i) {
			function->parameters[i].offset = offset;
			offset += ketl_type_get_stack_size(function->parameters[i].type);
		}
		return function;
	}
	else {
		if (!byTraits->initialized) {
			byTraits->initialized = true;
			ketl_int_map_init(&byTraits->children, sizeof(TypeFunctionSearchNode), 16);
		}
		return getTypeFunction(state, &byTraits->children, parameters, lastParameter, currentParameter + 1);
	}
}

static void initNamespace(ketl_namespace* namespace) {
	ketl_int_map_init(&namespace->types, sizeof(ketl_type_pointer), 16);
	ketl_int_map_init(&namespace->variables, sizeof(ketl_ir_undefined_value*), 16);
	ketl_int_map_init(&namespace->namespaces, sizeof(ketl_namespace), 8);
}

#include <stdlib.h>

static void deinitNamespace(ketl_namespace* namespace) {	
	{
		ketl_int_map_iterator childrenIterator;
		ketl_int_map_iterator_init(&childrenIterator, &namespace->namespaces);
		while (ketl_int_map_iterator_has_next(&childrenIterator)) {
			const char* name;
			ketl_namespace* child;
			ketl_int_map_iterator_get(&childrenIterator, (ketl_int_map_key*)(&name), &child);

			deinitNamespace(child);
			ketl_int_map_iterator_next(&childrenIterator);
		}
	}
	
	ketl_int_map_deinit(&namespace->namespaces);
	ketl_int_map_deinit(&namespace->variables);
	ketl_int_map_deinit(&namespace->types);
}

static void createPrimitive(ketl_type_primitive* type, const char* name, uint64_t size, ketl_namespace* globalNamespace) {
	type->name = name;
	type->kind = KETL_TYPE_KIND_PRIMITIVE;
	type->size = (uint32_t)size;

	ketl_type_pointer* typePtr;
	ketl_int_map_get_or_create(&globalNamespace->types, (ketl_int_map_key)name, &typePtr);
	typePtr->primitive = type;
}

static void registerPrimitiveBinaryOperator(ketl_state* state, ketl_operator_code operatorCode, ketl_ir_operation_code operationCode, ketl_type_primitive* type) {
	ketl_binary_operator** pOperator;
	if (ketl_int_map_get_or_create(&state->binaryOperators, operatorCode, &pOperator)) {
		*pOperator = NULL;
	}

	ketl_binary_operator operator;
	ketl_variable_traits traits;

	traits.values.isNullable = false;
	traits.values.isConst = true;

	operator.code = operationCode;

	operator.lhsTraits = traits;
	operator.rhsTraits = traits;
	operator.outputTraits = traits;

	operator.lhsType.primitive = type;
	operator.rhsType.primitive = type;
	operator.outputType.primitive = type;

	operator.next = *pOperator;
	ketl_binary_operator* newOperator = ketl_object_pool_get(&state->binaryOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;
}

static void registerPrimitiveComparisonOperator(ketl_state* state, ketl_operator_code operatorCode, ketl_ir_operation_code operationCode, ketl_type_primitive* type) {
	ketl_binary_operator** pOperator;
	if (ketl_int_map_get_or_create(&state->binaryOperators, operatorCode, &pOperator)) {
		*pOperator = NULL;
	}

	ketl_binary_operator operator;
	ketl_variable_traits traits;

	traits.values.isNullable = false;
	traits.values.isConst = true;

	operator.code = operationCode;

	operator.lhsTraits = traits;
	operator.rhsTraits = traits;
	operator.outputTraits = traits;

	operator.lhsType.primitive = type;
	operator.rhsType.primitive = type;
	operator.outputType.primitive = &state->primitives.bool_t;

	operator.next = *pOperator;
	ketl_binary_operator* newOperator = ketl_object_pool_get(&state->binaryOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;
}

static void registerPrimitiveCastOperator(ketl_state* state, ketl_type_primitive* sourceType, ketl_type_primitive* targetType, ketl_ir_operation_code operationCode, bool implicit) {
	ketl_cast_operator** pOperator;
	if (ketl_int_map_get_or_create(&state->castOperators, (ketl_int_map_key)sourceType, &pOperator)) {
		*pOperator = NULL;
	}

	ketl_cast_operator operator;
	ketl_variable_traits traits;

	traits.values.isNullable = false;
	traits.values.isConst = true;

	operator.code = operationCode;

	operator.inputTraits = traits;
	operator.outputTraits = traits;

	operator.outputType.primitive = targetType;
	operator.implicit = implicit;

	operator.next = *pOperator;
	ketl_cast_operator* newOperator = ketl_object_pool_get(&state->castOperatorsPool);
	*newOperator = operator;
	*pOperator = newOperator;

}

void ketl_state_destroy(ketl_state* state) {
	{
		ketl_int_map_iterator childrenIterator;
		ketl_int_map_iterator_init(&childrenIterator, &state->typeFunctionSearchMap);
		while (ketl_int_map_iterator_has_next(&childrenIterator)) {
			ketl_type_pointer type;
			TypeFunctionSearchNode* child;
			ketl_int_map_iterator_get(&childrenIterator, (ketl_int_map_key*)(&type.base), &child);

			deinitTypeFunctionSearchNode(child);
			ketl_int_map_iterator_next(&childrenIterator);
		}
	}

	ketl_int_map_deinit(&state->typeFunctionSearchMap);
	ketl_object_pool_deinit(&state->typeFunctionsPool);
	ketl_object_pool_deinit(&state->typeParametersPool);

	// TODO destruct variables in reversed order
	ketl_object_pool_deinit(&state->variablesPool);
	ketl_object_pool_deinit(&state->undefVarPool);

	ketl_int_map_deinit(&state->castOperators);
	ketl_int_map_deinit(&state->binaryOperators);
	ketl_int_map_deinit(&state->unaryOperators);
	ketl_object_pool_deinit(&state->castOperatorsPool);
	ketl_object_pool_deinit(&state->binaryOperatorsPool);
	ketl_object_pool_deinit(&state->unaryOperatorsPool);

	deinitNamespace(&state->globalNamespace);
	ketl_ir_compiler_deinit(&state->irCompiler);
	ketl_compiler_deinit(&state->compiler);

	ketl_heap_memory_deinit(&state->hmemory);
	ketl_atomic_strings_deinit(&state->strings);

	free(state);
}

ketl_state* ketl_state_create() {
	ketl_state* state = malloc(sizeof(ketl_state));

	ketl_atomic_strings_init(&state->strings, 16);
	ketl_heap_memory_init(&state->hmemory);

	ketl_compiler_init(&state->compiler, state);
	ketl_ir_compiler_init(&state->irCompiler);
	initNamespace(&state->globalNamespace);

	ketl_object_pool_init(&state->unaryOperatorsPool, sizeof(ketl_unary_operator), 8);
	ketl_object_pool_init(&state->binaryOperatorsPool, sizeof(ketl_binary_operator), 8);
	ketl_object_pool_init(&state->castOperatorsPool, sizeof(ketl_cast_operator), 8);
	ketl_int_map_init(&state->unaryOperators, sizeof(ketl_unary_operator*), 4);
	ketl_int_map_init(&state->binaryOperators, sizeof(ketl_binary_operator*), 4);
	ketl_int_map_init(&state->castOperators, sizeof(ketl_cast_operator*), 4);

	ketl_object_pool_init(&state->undefVarPool, sizeof(ketl_ir_undefined_value), 16);
	ketl_object_pool_init(&state->variablesPool, sizeof(ketl_ir_variable), 16);

	ketl_object_pool_init(&state->typeParametersPool, sizeof(ketl_type_parameters), 16);
	ketl_object_pool_init(&state->typeFunctionsPool, sizeof(ketl_type_function), 16);
	ketl_int_map_init(&state->typeFunctionSearchMap, sizeof(TypeFunctionSearchNode), 16);

	createPrimitive(&state->primitives.void_t, ketl_atomic_strings_get(&state->strings, "void", KETL_NULL_TERMINATED_LENGTH), 0, &state->globalNamespace);
	createPrimitive(&state->primitives.bool_t, ketl_atomic_strings_get(&state->strings, "bool", KETL_NULL_TERMINATED_LENGTH), sizeof(bool), &state->globalNamespace);
	createPrimitive(&state->primitives.i8_t, ketl_atomic_strings_get(&state->strings, "i8", KETL_NULL_TERMINATED_LENGTH), sizeof(int8_t), &state->globalNamespace);
	createPrimitive(&state->primitives.i16_t, ketl_atomic_strings_get(&state->strings, "i16", KETL_NULL_TERMINATED_LENGTH), sizeof(int16_t), &state->globalNamespace);
	createPrimitive(&state->primitives.i32_t, ketl_atomic_strings_get(&state->strings, "i32", KETL_NULL_TERMINATED_LENGTH), sizeof(int32_t), &state->globalNamespace);
	createPrimitive(&state->primitives.i64_t, ketl_atomic_strings_get(&state->strings, "i64", KETL_NULL_TERMINATED_LENGTH), sizeof(int64_t), &state->globalNamespace);
	createPrimitive(&state->primitives.f32_t, ketl_atomic_strings_get(&state->strings, "f32", KETL_NULL_TERMINATED_LENGTH), sizeof(float), &state->globalNamespace);
	createPrimitive(&state->primitives.f64_t, ketl_atomic_strings_get(&state->strings, "f64", KETL_NULL_TERMINATED_LENGTH), sizeof(double), &state->globalNamespace);

	state->metaTypes.functionClass = (ketl_type_meta){.name = NULL, .kind = KETL_TYPE_KIND_META, .size = sizeof(ketl_type_function_class)};

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

	return state;
}

static void ketlDefineVariable(ketl_state* state, const char* name, ketl_type_pointer type, ketl_variable_traits traits, void* pointer) {
	ketl_ir_variable* variable = ketl_object_pool_get(&state->variablesPool);
	variable->type = type;
	variable->traits = traits;
	variable->value.type = KETL_IR_ARGUMENT_TYPE_POINTER;
	variable->value.pointer = pointer;

	ketl_ir_undefined_value* uvalue = ketl_object_pool_get(&state->undefVarPool);
	uvalue->variable = variable;
	uvalue->scopeIndex = 0;
	uvalue->next = NULL;

	const char* uniqName = ketl_atomic_strings_get(&state->strings, name, KETL_NULL_TERMINATED_LENGTH);

	ketl_ir_undefined_value** ppUValue;
	ketl_int_map_get_or_create(&state->globalNamespace.variables, (ketl_int_map_key)uniqName, &ppUValue);
	*ppUValue = uvalue;
}

void ketl_state_define_external_variable(ketl_state* state, const char* name, ketl_type_pointer type, void* pointer) {
	ketl_variable_traits traits;
	traits.values.isConst = false;
	traits.values.isNullable = false;
	ketlDefineVariable(state, name, type, traits, pointer);
}

void* ketl_state_define_internal_variable(ketl_state* state, const char* name, ketl_type_pointer type) {
	ketl_variable_traits traits;
	traits.values.isConst = false;
	traits.values.isNullable = false;
	void* pointer = ketl_heap_memory_allocate_root(&state->hmemory, type);
	ketlDefineVariable(state, name, type, traits, pointer);
	return pointer;
}

static ketl_function* ketl_state_compile_function_impl(ketl_state* state, ketl_namespace* namespace, const char* source, ketl_function_parameter* parameters, uint64_t parametersCount);

ketl_function* ketlCompileFunction(ketl_state* state, const char* source, ketl_function_parameter* parameters, uint64_t parametersCount) {
	return ketl_state_compile_function_impl(state, NULL, source, parameters, parametersCount);
}

int64_t ketl_state_eval_local(ketl_state* state, const char* source) {
	ketl_function* function = ketl_state_compile_function_impl(state, NULL, source, NULL, 0);
	return function ? KETL_CALL_FUNCTION(function, int64_t) : -1;
}


int64_t ketl_state_eval(ketl_state* state, const char* source) {
	ketl_function* function = ketl_state_compile_function_impl(state, &state->globalNamespace, source, NULL, 0);
	return function ? KETL_CALL_FUNCTION(function, int64_t) : -1;
}

ketl_function* ketl_state_compile_function_impl(ketl_state* state, ketl_namespace* namespace, const char* source, ketl_function_parameter* parameters, uint64_t parametersCount) {
	ketl_syntax_node* root = ketl_syntax_solver_solve(source, KETL_NULL_TERMINATED_LENGTH, &state->compiler.bytecodeCompiler.syntaxSolver, &state->compiler.bytecodeCompiler.syntaxNodePool);
	if (!root) {
		return NULL;
	}

	ketl_ir_function_definition irFunction = ketl_ir_builder_build(&state->compiler.irBuilder, namespace, KETL_CREATE_TYPE_PTR(NULL), root, parameters, parametersCount);
	if (irFunction.function == NULL) {
		return NULL;
	}

	// TODO optimization on ir

	ketl_type_function* functionType = irFunction.type.functionClass->functionType;
	const uint8_t* functionBytecode = ketl_ir_compiler_compile(&state->irCompiler, irFunction.function, 
		functionType->parameters + 1, functionType->parameterCount - 1);
	
	ketl_function* functionClass = ketl_heap_memory_allocate(&state->hmemory, irFunction.type);
	*(const uint8_t**)functionClass = functionBytecode;
	
	free(irFunction.function);
	return functionClass;
}

ketl_variable* ketl_state_compile_function(ketl_state* state, const char* source, ketl_function_parameter* parameters, uint64_t parametersCount) {
	ketl_syntax_node* root = ketl_syntax_solver_solve(source, KETL_NULL_TERMINATED_LENGTH, &state->compiler.bytecodeCompiler.syntaxSolver, &state->compiler.bytecodeCompiler.syntaxNodePool);
	if (!root) {
		return NULL;
	}

	ketl_ir_function_definition irFunction = ketl_ir_builder_build(&state->compiler.irBuilder, NULL, KETL_CREATE_TYPE_PTR(NULL), root, parameters, parametersCount);
	if (irFunction.function == NULL) {
		return NULL;
	}

	// TODO optimization on ir

	ketl_type_function* functionType = irFunction.type.functionClass->functionType;
	const uint8_t*  functionBytecode = ketl_ir_compiler_compile(&state->irCompiler, irFunction.function, 
		functionType->parameters + 1, functionType->parameterCount - 1);
	
	ketl_function* functionClass = ketl_heap_memory_allocate(&state->hmemory, irFunction.type);
	*(const uint8_t**)functionClass = functionBytecode;
	(void)functionClass;

	free(irFunction.function);

	ketl_variable* variable = NULL;

	//variable->value.pointer = function;
	//variable->value.type = KETL_IR_ARGUMENT_TYPE_POINTER;

	return variable;
}

ketl_type_pointer ketl_state_get_type_i32(ketl_state* state) {
	return KETL_CREATE_TYPE_PTR(&state->primitives.i32_t);
}

ketl_type_pointer ketl_state_get_type_i64(ketl_state* state) {
	return KETL_CREATE_TYPE_PTR(&state->primitives.i64_t);
}

ketl_type_pointer getFunctionType(ketl_state* state, ketl_type_parameters* parameters, uint64_t parametersCount) {
	ketl_type_pointer result;
	result.function = getTypeFunction(state, &state->typeFunctionSearchMap, parameters, parametersCount - 1, 0);
	return result;
}

