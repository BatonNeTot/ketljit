//🫖ketl
#include "ketl/type.h"

#include "type_impl.h"

uint64_t ketl_type_get_stack_size(ketl_variable_traits traits, ketl_type_pointer type) {
	// TODO!! not sure about ref, its not always shows proper stack size now
	return traits.values.type >= KETL_TRAIT_TYPE_REF || traits.values.isNullable ||
		type.base->kind == KETL_TYPE_KIND_CLASS || type.base->kind == KETL_TYPE_KIND_FUNCTION ? sizeof(void*) : type.primitive->size;
}

uint64_t ketl_type_get_size(ketl_variable_traits traits, ketl_type_pointer type) {
	(void)traits;
	switch (type.base->kind) {
		case KETL_TYPE_KIND_PRIMITIVE: {
			return type.primitive->size;
		}
		case KETL_TYPE_KIND_FUNCTION_CLASS: {
			return type.functionClass->size;
		}
		case KETL_TYPE_KIND_CLASS: {
			return type.clazz->size;
		}
	}
	return 0;
}