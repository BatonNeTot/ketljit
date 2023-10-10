//🍲ketl
#ifndef managed_memory_h
#define managed_memory_h

#include "ketl/object_pool.h"
#include "ketl/utils.h"

KETL_DEFINE(KETLManagedMemory) {
	uint64_t pagePool;
	uint64_t currentOffset;
};

void ketlInitManagedMemory(KETLManagedMemory* managedMemory);

void ketlDeinitManagedMemory(KETLManagedMemory* managedMemory);

uint8_t* ketlManagedMemoryAllocate(KETLManagedMemory* managedMemory, uint64_t size);

uint8_t* ketlManagedMemoryReset(KETLManagedMemory* managedMemory);

#endif /*managed_memory_h*/
