//🫖ketl
#ifndef ketl_function_h
#define ketl_function_h

#include "ketl/type.h"

#include "ketl/utils.h"

KETL_FORWARD(ketl_function);

KETL_DEFINE(ketl_function_parameter) {
	const char* name;
	ketl_type_pointer type;
	ketl_variable_traits traits;
};

#define KETL_CALL_FUNCTION(function, returnType) ((*(returnType(**)(ketl_function*))(function))((function)))
#define KETL_CALL_FUNCTION_ARGS(function, returnType, ...) ((*(returnType(**)(ketl_function*, ...))(function))((function), __VA_ARGS__))

#endif // ketl_function_h
