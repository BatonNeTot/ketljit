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

#define KETL_CALL_FUNCTION(function, cast, ...) (((cast)(function))(__VA_ARGS__))

#endif /*function_h*/
