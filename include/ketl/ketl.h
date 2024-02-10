//🫖ketl
#ifndef ketl_ketl_h
#define ketl_ketl_h

#include "ketl/type.h"
#include "ketl/variable.h"
#include "ketl/function.h"

#include "ketl/utils.h"

KETL_FORWARD(ketl_state);

ketl_state* ketl_state_create();

void ketl_state_destroy(ketl_state* state);

ketl_variable* ketl_state_define_external_variable(ketl_state* state, const char* name, ketl_type_pointer type, void* pointer);

void* ketl_state_define_internal_variable(ketl_state* state, const char* name, ketl_type_pointer type);

ketl_type_pointer ketl_state_get_type_i32(ketl_state* state);

ketl_type_pointer ketl_state_get_type_i64(ketl_state* state);

ketl_type_pointer ketl_state_get_type_function(ketl_state* state, ketl_variable_features output, ketl_variable_features* parameters, uint64_t parametersCount);

ketl_variable* ketl_state_compile_function(ketl_state* state, const char* source, ketl_function_parameter* parameters, uint64_t parametersCount);

ketl_variable* ketl_state_eval_local(ketl_state* state, const char* source);

ketl_variable* ketl_state_eval(ketl_state* state, const char* source);

#endif // ketl_ketl_h
