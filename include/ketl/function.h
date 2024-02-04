//🫖ketl
#ifndef ketl_function_h
#define ketl_function_h

#include "ketl/type.h"

#include "ketl/utils.h"

KETL_DEFINE(ketl_function_parameter) {
	const char* name;
	ketl_type_pointer type;
	ketl_variable_traits traits;
};

#endif // ketl_function_h
