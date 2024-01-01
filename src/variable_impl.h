//🫖ketl
#ifndef ketl_variable_impl_h
#define ketl_variable_impl_h

#include "ketl/variable.h"
#include "ketl/type.h"
#include "compiler/ir_node.h"

KETL_DEFINE(ketl_ir_variable) {
    ketl_type_pointer type;
	ketl_variable_traits traits;
	ketl_ir_argument value;
};

KETL_DEFINE(ketl_variable_cb) {
	ketl_ir_variable* variable;
	uint64_t userCount;
};

KETL_DEFINE(ketl_variable) {
    ketl_variable_cb* variableCB;
};

#endif // ketl_variable_impl_h