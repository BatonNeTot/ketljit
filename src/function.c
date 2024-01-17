//🫖ketl
#include "ketl/function.h"

#include "type_impl.h"
#include "ketl_impl.h"

typedef int64_t(*t_function_with_argument)(int64_t);

typedef void(*function_void_t)();

typedef int64_t(*function_i64_t)();

struct __function_convertor {
	union {
		ketl_function* object;
		t_function_with_argument t_function_with_argument;
		function_void_t function_void_t;
		function_i64_t function_i64_t;
	} base;
};

#define CAST_FUNCTION(function, targetType) (((struct __function_convertor){{function}}).base.targetType)

void ketlCallFunction(ketl_function* function, void* returnPtr) {
	(void)returnPtr;
	function_i64_t callableFunction = CAST_FUNCTION(function, function_i64_t);
	callableFunction();
}

void ketlCallFunctionWithArgument(ketl_function* function, void* returnPtr, int64_t argument) {
	(void)returnPtr;
	t_function_with_argument callableFunction = CAST_FUNCTION(function, t_function_with_argument);
	callableFunction(argument);
}

void ketlCallFunctionOfType(ketl_state* state, ketl_function* function, void* returnPtr, ketl_type_pointer type, ...) {
	if (type.base->kind != KETL_TYPE_KIND_FUNCTION) {
		return;
	}

	ketl_type_pointer returnType = type.function->parameters[0].type;
	if (returnType.primitive == &state->primitives.void_t) {
		if (type.function->parameterCount == 0) {
			CAST_FUNCTION(function, function_void_t)();
			return;
		}


	}
	else {
		if (type.function->parameterCount == 0) {
			*(int64_t*)returnPtr = CAST_FUNCTION(function, function_i64_t)();
			return;
		}

	}
}