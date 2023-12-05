//🍲ketl
#include "ketl/type.h"

#include "ketl/type_impl.h"

uint64_t getStackTypeSize(KETLVariableTraits traits, KETLTypePtr type) {
	// TODO!! not sure about ref, its not always shows proper stack size now
	return traits.values.type >= KETL_TRAIT_TYPE_REF || traits.values.isNullable ||
		type.base->kind == KETL_TYPE_KIND_CLASS || type.base->kind == KETL_TYPE_KIND_FUNCTION ? sizeof(void*) : type.primitive->size;
}