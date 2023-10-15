//🍲ketl
#ifndef function_h
#define function_h

#include "ketl/type.h"

#include "ketl/utils.h"

#include <inttypes.h>

KETL_FORWARD(KETLFunction);
KETL_FORWARD(KETLState);

KETL_DEFINE(KETLParameter) {
	const char* name;
	KETLTypePtr type;
	KETLVariableTraits traits;
};

void ketlCallFunction(KETLFunction* function, void* returnPtr);

void ketlCallFunctionOfType(KETLState* state, KETLFunction* function, void* returnPtr, KETLTypePtr type, ...);

// TODO replace with vararg when introduce type
void ketlCallFunctionWithArgument(KETLFunction* function, void* returnPtr, int64_t argument);

#endif /*function_h*/
