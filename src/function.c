//🍲ketl
#include "ketl/function.h"

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