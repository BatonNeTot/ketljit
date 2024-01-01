//🫖ketl
#ifndef ketl_ketl_impl_h
#define ketl_ketl_impl_h

#include "ketl/ketl.h"

#include "ketl/variable.h"
#include "ketl/function.h"
#include "type_impl.h"
#include "operators.h"

#include "compiler/ir_compiler.h"
#include "compiler/compiler.h"

#include "containers/atomic_strings.h"
#include "containers/object_pool.h"
#include "containers/int_map.h"

KETL_DEFINE(ketl_namespace) {
	ketl_int_map variables;
	ketl_int_map namespaces;
	ketl_int_map types;
};

KETL_DEFINE(ketl_unary_operator) {
	ketl_ir_operation_code code;
	ketl_variable_traits inputTraits;
	ketl_variable_traits outputTraits;
	ketl_type_pointer inputType;
	ketl_type_pointer outputType;
	ketl_unary_operator* next;
};

KETL_DEFINE(ketl_binary_operator) {
	ketl_ir_operation_code code;
	ketl_variable_traits lhsTraits;
	ketl_variable_traits rhsTraits;
	ketl_variable_traits outputTraits;
	ketl_type_pointer lhsType;
	ketl_type_pointer rhsType;
	ketl_type_pointer outputType;
	ketl_binary_operator* next;
};

KETL_DEFINE(ketl_cast_operator) {
	ketl_ir_operation_code code;
	bool implicit;
	ketl_variable_traits inputTraits;
	ketl_variable_traits outputTraits;
	ketl_type_pointer outputType;
	ketl_cast_operator* next;
};

KETL_DEFINE(ketl_state) {
	ketl_atomic_strings strings;
	ketl_compiler compiler;
	ketl_ir_compiler irCompiler;
	ketl_namespace globalNamespace;
	struct {
		ketl_type_primitive void_t;
		ketl_type_primitive bool_t;
		ketl_type_primitive i8_t;
		ketl_type_primitive i16_t;
		ketl_type_primitive i32_t;
		ketl_type_primitive i64_t;
		ketl_type_primitive u8_t;
		ketl_type_primitive u16_t;
		ketl_type_primitive u32_t;
		ketl_type_primitive u64_t;
		ketl_type_primitive f32_t;
		ketl_type_primitive f64_t;
	} primitives;

	ketl_object_pool unaryOperatorsPool;
	ketl_object_pool binaryOperatorsPool;
	ketl_object_pool castOperatorsPool;
	ketl_int_map unaryOperators;
	ketl_int_map binaryOperators;
	ketl_int_map castOperators;

	ketl_object_pool undefVarPool;
	ketl_object_pool variablesPool;

	ketl_object_pool typeParametersPool;
	ketl_object_pool typeFunctionsPool;
	ketl_int_map typeFunctionSearchMap;
};

#endif // ketl_ketl_impl_h