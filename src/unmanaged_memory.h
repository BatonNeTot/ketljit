//🫖ketl
#ifndef ketl_unmanaged_memory_h
#define ketl_unmanaged_memory_h

#include "containers/object_pool.h"
#include "ketl/utils.h"

KETL_FORWARD(ketl_unmanaged_memory_page);

KETL_DEFINE(KETLUnmanagedMemory) {
	ketl_object_pool pagePool;
	ketl_unmanaged_memory_page* currentPage;
	uint64_t currentOffset;
};

void ketl_unmanaged_memory_init(KETLUnmanagedMemory* unmanagedMemory);

void ketl_unmanaged_memory_deinit(KETLUnmanagedMemory* unmanagedMemory);

uint8_t* ketl_unmanaged_memory_allocate(KETLUnmanagedMemory* unmanagedMemory, uint64_t size);

uint8_t* ketl_unmanaged_memory_reset(KETLUnmanagedMemory* unmanagedMemory);

#endif // ketl_unmanaged_memory_h
