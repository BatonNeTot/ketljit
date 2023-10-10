//🍲ketl
#ifndef unmanaged_memory_h
#define unmanaged_memory_h

#include "ketl/object_pool.h"
#include "ketl/utils.h"

KETL_FORWARD(KETLUnmanagedMemoryPage);

KETL_DEFINE(KETLUnmanagedMemory) {
	KETLObjectPool pagePool;
	KETLUnmanagedMemoryPage* currentPage;
	uint64_t currentOffset;
};

void ketlInitUnmanagedMemory(KETLUnmanagedMemory* unmanagedMemory);

void ketlDeinitUnmanagedMemory(KETLUnmanagedMemory* unmanagedMemory);

uint8_t* ketlUnmanagedMemoryAllocate(KETLUnmanagedMemory* unmanagedMemory, uint64_t size);

uint8_t* ketlUnmanagedMemoryReset(KETLUnmanagedMemory* unmanagedMemory);

#endif /*unmanaged_memory_h*/
