//🫖ketl
#ifndef ketl_variable_impl_h
#define ketl_variable_impl_h

#include "ketl/variable.h"
#include "ketl/type.h"
#include "compiler/ir_node.h"

KETL_FORWARD(ketl_state);

KETL_DEFINE(ketl_ir_variable) {
	ketl_ir_argument value;
    ketl_type_pointer type;
	ketl_variable_traits traits;
};

KETL_DEFINE(ketl_variable) {
    ketl_ir_variable irVariable;
	ketl_state* state;
};

#endif // ketl_variable_impl_h