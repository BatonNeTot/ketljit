//🍲ketl
#ifndef function_h
#define function_h

#include "ketl/utils.h"

#include <inttypes.h>

KETL_FORWARD(KETLFunction);

void ketlCallFunction(KETLFunction* function, void* returnPtr);

// TODO replace with vararg when introduce type
void ketlCallFunctionWithArgument(KETLFunction* function, void* returnPtr, int64_t argument);

#endif /*function_h*/
