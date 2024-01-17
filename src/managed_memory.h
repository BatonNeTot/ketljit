//🫖ketl
#ifndef ketl_managed_memory_h
#define ketl_managed_memory_h

#include "type_impl.h"

#include "ketl/utils.h"

typedef void* (*ketl_mmemory_alloc_f)(ketl_type_pointer type, void* userInfo);
typedef void (*ketl_mmemory_reg_unmanaged_f)(void* ptr, ketl_type_pointer type, void* userInfo);
typedef void (*ketl_mmemory_add_ref_anchor_f)(void* ptr, void* anchor, void* userInfo);
typedef void (*ketl_mmemory_destroy_f)(void* ptr, void* userInfo);

KETL_DEFINE(ketl_mmemory) {
	ketl_mmemory_alloc_f alloc;
	ketl_mmemory_destroy_f destroy;
	void* userInfo;
};

#endif // ketl_managed_memory_h
