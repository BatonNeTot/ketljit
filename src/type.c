//🫖ketl
#include "ketl/type.h"

#include "type_impl.h"

uint64_t ketl_type_get_stack_size(ketl_variable_traits traits, ketl_type_pointer type) {
	// TODO!! not sure about ref, its not always shows proper stack size now
	return traits.values.type >= KETL_TRAIT_TYPE_REF || traits.values.isNullable ||
		type.base->kind == KETL_TYPE_KIND_CLASS || type.base->kind == KETL_TYPE_KIND_FUNCTION ? sizeof(void*) : type.primitive->size;
}