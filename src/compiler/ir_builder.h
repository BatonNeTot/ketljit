//🫖ketl
#ifndef ketl_compiler_ir_builder_h
#define ketl_compiler_ir_builder_h

#include "ir_node.h"

#include "compiler/syntax_node.h"
#include "ketl/type.h"
#include "function_impl.h"
#include "variable_impl.h"

#include "containers/object_pool.h"
#include "containers/int_map.h"
#include "containers/stack.h"

#include "ketl/utils.h"

KETL_FORWARD(ketl_state);
KETL_FORWARD(ketl_namespace);
KETL_FORWARD(ketl_ir_variable);
KETL_FORWARD(ketl_scoped_variable);

KETL_DEFINE(ketl_ir_undefined_value) {
	ketl_ir_variable* variable;
	uint64_t scopeIndex;
	ketl_ir_undefined_value* next;
};

KETL_DEFINE(ketl_ir_undefined_delegate) {
	ketl_ir_undefined_value* caller;
	ketl_ir_undefined_value* arguments;
	ketl_ir_undefined_value* next;
};

KETL_DEFINE(ketl_ir_return_node) {
	ketl_ir_operation* operation;
	ketl_ir_undefined_delegate* returnVariable;
	ketl_scoped_variable* tempVariable;
	ketl_ir_return_node* next;
};

KETL_DEFINE(ketl_ir_builder) {
	ketl_int_map variablesMap;

	ketl_object_pool scopedVariablesPool;
	ketl_object_pool variablesPool;
	ketl_object_pool operationsPool;
	ketl_object_pool udelegatePool;
	ketl_object_pool uvaluePool;

	ketl_object_pool argumentPointersPool;
	ketl_object_pool argumentsPool;

	ketl_object_pool castingPool;
	ketl_object_pool castingReturnPool;

	ketl_int_map operationReferMap;
	ketl_int_map argumentsMap;
	ketl_stack extraNextStack;

	ketl_object_pool returnPool;

	ketl_state* state;
};

KETL_DEFINE(ketl_ir_operation_range) {
	ketl_ir_operation* root;
	ketl_ir_operation* next;
};

KETL_DEFINE(KETLIRFunctionWIP) {
	ketl_ir_builder* builder;
	ketl_ir_operation_range operationRange;

	union {
		struct {
			ketl_scoped_variable* stackRoot;
			ketl_scoped_variable* currentStack;
		} stack;
		struct {
			ketl_scoped_variable* tempVariables;
			ketl_scoped_variable* localVariables;
		} vars;
	};

	uint64_t scopeIndex;
	ketl_ir_return_node* returnOperations;
	bool buildFailed;
};

KETL_DEFINE(ketl_ir_function_definition) {
	ketl_ir_function* function;
	ketl_type_pointer type;
};

void ketl_ir_builder_init(ketl_ir_builder* irBuilder, ketl_state* state);

void ketl_ir_builder_deinit(ketl_ir_builder* irBuilder);

ketl_ir_function_definition ketl_ir_builder_build(ketl_ir_builder* irBuilder, ketl_namespace* ketlNamespace, ketl_type_pointer returnType, ketl_syntax_node* syntaxNodeRoot, ketl_function_parameter* parameters, uint64_t parametersCount);

#endif // ketl_compiler_ir_builder_h
