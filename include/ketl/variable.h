//🫖ketl
#ifndef ketl_variable_h
#define ketl_variable_h

#include "ketl/utils.h"

KETL_FORWARD(ketl_variable);

int64_t ketl_variable_get_as_i64(ketl_variable* variable);

extern void ketl_variable_call_void(ketl_variable* variable, ...);

extern uint64_t ketl_variable_call_u64(ketl_variable* variable, ...);

extern int64_t ketl_variable_call_i64(ketl_variable* variable, ...);

void ketl_variable_free(ketl_variable* variable);

#endif // ketl_variable_h