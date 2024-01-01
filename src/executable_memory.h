//🫖ketl
#ifndef ketl_executable_memory_h
#define ketl_executable_memory_h

#include "containers/object_pool.h"
#include "ketl/utils.h"

KETL_FORWARD(ketl_executable_memory_page);

KETL_DEFINE(ketl_executable_memory) {
	ketl_object_pool pagePool;
	ketl_executable_memory_page* currentPage;
	uint32_t pageSize;
	uint32_t pageSizeLog;
	uint32_t currentOffset;
};

void ketl_executable_memory_init(ketl_executable_memory* exeMemory);

void ketl_executable_memory_deinit(ketl_executable_memory* exeMemory);

uint8_t* ketl_executable_memory_allocate(ketl_executable_memory* exeMemory, const uint8_t** opcodes, uint64_t blocksize, uint64_t length);

#endif // ketl_executable_memory_h
