//🍲ketl
#include "executable_memory.h"

#include <Windows.h>


KETL_DEFINE(KETLExecutableMemoryPage) {
	uint8_t* page;
	uint32_t pageSize;
};

void ketlInitExecutableMemory(KETLExecutableMemory* exeMemory) {
	ketlInitObjectPool(&exeMemory->pagePool, sizeof(KETLExecutableMemoryPage), 16);
	exeMemory->currentPage = NULL;

	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	exeMemory->pageSize = system_info.dwPageSize;
	uint32_t pageSizeLog = -1;
	while (system_info.dwPageSize > 0) {
		system_info.dwPageSize >>= 1;
		++pageSizeLog;
	}
	exeMemory->pageSizeLog = pageSizeLog;
}

void ketlDeinitExecutableMemory(KETLExecutableMemory* exeMemory) {
	KETLObjectPoolIterator pageIterator;
	ketlInitPoolIterator(&pageIterator, &exeMemory->pagePool);

	while (ketlIteratorPoolHasNext(&pageIterator)) {
		KETLExecutableMemoryPage page = *(KETLExecutableMemoryPage*)ketlIteratorPoolGetNext(&pageIterator);
		VirtualFree(page.page, 0, MEM_RELEASE);
	}

	ketlDeinitObjectPool(&exeMemory->pagePool);
}

uint8_t* ketlExecutableMemoryAllocate(KETLExecutableMemory* exeMemory, const uint8_t** opcodes, uint64_t blocksize, uint64_t length) {
	KETLExecutableMemoryPage* pCurrentPage = exeMemory->currentPage;
	uint32_t currentOffset = exeMemory->currentOffset;
	KETLExecutableMemoryPage currentPage;
	if (pCurrentPage == NULL) {
		uint32_t pageSize = exeMemory->pageSize;
		uint32_t requestedPageCount = (uint32_t)((length + (pageSize - 1)) >> exeMemory->pageSizeLog);

		currentPage.pageSize = pageSize * requestedPageCount;
		currentPage.page = VirtualAlloc(NULL, currentPage.pageSize, MEM_COMMIT, PAGE_READWRITE);

		pCurrentPage = ketlGetFreeObjectFromPool(&exeMemory->pagePool);
		*pCurrentPage = currentPage;
		exeMemory->currentPage = pCurrentPage;
		currentOffset = 0;
	}
	currentPage = *pCurrentPage;
	if (currentOffset + length > pCurrentPage->pageSize) {
		uint32_t pageSize = exeMemory->pageSize;
		uint32_t requestedPageCount = (uint32_t)((length + (pageSize - 1)) >> exeMemory->pageSizeLog);

		currentPage.pageSize = pageSize * requestedPageCount;
		currentPage.page = VirtualAlloc(NULL, currentPage.pageSize, MEM_COMMIT, PAGE_READWRITE);

		pCurrentPage = ketlGetFreeObjectFromPool(&exeMemory->pagePool);
		*pCurrentPage = currentPage;
		exeMemory->currentPage = pCurrentPage;
		currentOffset = 0;
	}
	else {
		DWORD dummy;
		VirtualProtect(currentPage.page, currentPage.pageSize, PAGE_READWRITE, &dummy);
	}

	uint32_t requestedMemory = (length + 15) & (-16);
	uint8_t* resultMemory = currentPage.page + currentOffset;
	uint8_t* memoryIterator = resultMemory;

	uint64_t i = 0;
	while (length > blocksize) {
		memcpy(memoryIterator, opcodes[i], blocksize);
		memoryIterator += blocksize;
		length -= blocksize;
		++i;
	}
	memcpy(memoryIterator, opcodes[i], length);
	exeMemory->currentOffset = currentOffset + requestedMemory;

	{
		DWORD dummy;
		VirtualProtect(currentPage.page, currentPage.pageSize, PAGE_EXECUTE_READ, &dummy);
	}

	return resultMemory;
}