//🍲ketl
#ifndef function_h
#define function_h

#include "ketl/utils.h"

#include <inttypes.h>

KETL_FORWARD(KETLFunction);

void ketlCallFunction(KETLFunction* function, volatile void* returnPtr);

#endif /*function_h*/
