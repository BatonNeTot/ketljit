//🍲ketl
#ifndef executable_memory_h
#define executable_memory_h

#include "ketl/object_pool.h"
#include "ketl/utils.h"

KETL_FORWARD(KETLExecutableMemoryPage);

KETL_DEFINE(KETLExecutableMemory) {
	KETLObjectPool pagePool;
	KETLExecutableMemoryPage* currentPage;
	uint32_t pageSize;
	uint32_t pageSizeLog;
	uint32_t currentOffset;
};

void ketlInitExecutableMemory(KETLExecutableMemory* exeMemory);

void ketlDeinitExecutableMemory(KETLExecutableMemory* exeMemory);

uint8_t* ketlExecutableMemoryAllocate(KETLExecutableMemory* exeMemory, const uint8_t** opcodes, uint64_t blocksize, uint64_t length);

#endif /*executable_memory_h*/
