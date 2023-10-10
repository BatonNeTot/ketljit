//🍲ketl
#ifndef memory_h
#define memory_h

#include "ketl/utils.h"

#include <inttypes.h>

typedef void* (*KETLAllocatorAlloc)(size_t size, void* userInfo);
typedef void* (*KETLAllocatorRealloc)(void* ptr, size_t size, void* userInfo);
typedef void (*KETLAllocatorFree)(void* ptr, void* userInfo);

KETL_DEFINE(KETLAllocator) {
	KETLAllocatorAlloc alloc;
	KETLAllocatorRealloc realloc;
	KETLAllocatorFree free;
	void* userInfo;
};

extern const KETLAllocator ketl_default_allocator;

#endif /*memory_h*/
