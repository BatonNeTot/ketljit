//🫖ketl
#include "ketl/type.h"

#include "type_impl.h"

uint64_t ketl_type_get_stack_size(ketl_type_pointer type) {
	return type.base->kind == KETL_TYPE_KIND_PRIMITIVE ? type.primitive->size : sizeof(void*);
}

uint64_t ketl_type_get_size(ketl_type_pointer type) {
	switch (type.base->kind) {
		case KETL_TYPE_KIND_META: {
			return type.meta->size;
		}
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