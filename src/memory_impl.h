//🫖ketl
#ifndef ketl_memory_impl_h
#define ketl_memory_impl_h

#include "ketl/memory.h"

inline void* ketl_alloc(ketl_allocator* allocator, size_t size) {
	return allocator->alloc(size, allocator->userInfo);
}

inline void* ketl_realloc(ketl_allocator* allocator, void* ptr, size_t size) {
	return allocator->realloc(ptr, size, allocator->userInfo);
}

inline void ketl_free(ketl_allocator* allocator, void* ptr) {
	allocator->free(ptr, allocator->userInfo);
}

#endif // ketl_memory_impl_h
