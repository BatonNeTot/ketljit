//🫖ketl
#ifndef ketl_managed_memory_h
#define ketl_managed_memory_h

#include "containers/object_pool.h"
#include "ketl/utils.h"

KETL_DEFINE(ketl_managed_memory) {
	uint64_t pagePool;
	uint64_t currentOffset;
};

void ketl_managed_memory_init(ketl_managed_memory* managedMemory);

void ketl_managed_memory_deinit(ketl_managed_memory* managedMemory);

uint8_t* ketl_managed_memory_allocate(ketl_managed_memory* managedMemory, uint64_t size);

uint8_t* ketl_managed_memory_reset(ketl_managed_memory* managedMemory);

#endif // ketl_managed_memory_h
