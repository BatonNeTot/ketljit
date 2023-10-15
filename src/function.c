//🍲ketl
#include "ketl/function.h"

#include "ketl/type_impl.h"
#include "ketl/ketl.h"

typedef int64_t(*t_function)();

void ketlCallFunction(KETLFunction* function, void* returnPtr) {
	t_function callableFunction = (t_function)function;
	*(int64_t*)callableFunction();
}

typedef int64_t(*t_function_with_argument)(int64_t);

void ketlCallFunctionWithArgument(KETLFunction* function, void* returnPtr, int64_t argument) {
	t_function_with_argument callableFunction = (t_function_with_argument)function;
	*(int64_t*)callableFunction(argument);
}


typedef void(*function_void_t)();

typedef int64_t(*function_i64_t)();

void ketlCallFunctionOfType(KETLState* state, KETLFunction* function, void* returnPtr, KETLTypePtr type, ...) {
	if (type.base->kind != KETL_TYPE_KIND_FUNCTION) {
		return;
	}

	KETLTypePtr returnType = type.function->parameters[0].type;
	if (returnType.primitive == &state->primitives.void_t) {
		if (type.function->parametersCount == 0) {
			((function_void_t)function)();
			return;
		}


	}
	else {
		if (type.function->parametersCount == 0) {
			*(int64_t*)returnPtr = ((function_i64_t)function)();
			return;
		}

	}
}