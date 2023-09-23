//🍲ketl
#include "ketl/function.h"

typedef int64_t(*t_function)();

void ketlCallFunction(KETLFunction* function, void* returnPtr) {
	t_function callableFunction = (t_function)function;
	*(int64_t*)returnPtr = callableFunction();
}