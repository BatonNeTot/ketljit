//🍲ketl
#include "executable_memory.h"

#include <string.h>

#if KETL_OS_LINUX

#include <unistd.h>
#include <sys/mman.h>

inline static uint32_t ketl_get_page_size() {
	return getpagesize();
}

inline static void* ketl_allocate_exe_memory(void* pMemHint, uint32_t size) {
	return mmap(pMemHint, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

inline static void ketl_deallocate_exe_memory(void* ptr, uint32_t size) {
	munmap(ptr, size);
}

inline static void ketl_protect_exe_memory(void* ptr, uint32_t size) {
	mprotect(ptr, size, PROT_READ | PROT_EXEC);
}

inline static void ketl_unprotect_exe_memory(void* ptr, uint32_t size) {
	mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

#endif


#if KETL_OS_WINDOWS
#include <Windows.h>

inline static uint32_t ketl_get_page_size() {
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwPageSize;
}

inline static void* ketl_allocate_exe_memory(void* pMemHint, uint32_t size) {
	VirtualAlloc(pMemHint, size, MEM_COMMIT, PAGE_READWRITE);
}

inline static void ketl_deallocate_exe_memory(void* ptr, uint32_t size) {
	(void)size;
	VirtualFree(ptr, 0, MEM_RELEASE);
}

inline static void ketl_protect_exe_memory(void* ptr, uint32_t size) {
	DWORD dummy;
	VirtualProtect(ptr, size, PAGE_READWRITE, &dummy);
}

inline static void ketl_unprotect_exe_memory(void* ptr, uint32_t size) {
	DWORD dummy;
	VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &dummy);
}

#endif

KETL_DEFINE(KETLExecutableMemoryPage) {
	uint8_t* page;
	uint32_t pageSize;
};

void ketlInitExecutableMemory(KETLExecutableMemory* exeMemory) {
	ketlInitObjectPool(&exeMemory->pagePool, sizeof(KETLExecutableMemoryPage), 16);
	exeMemory->currentPage = NULL;

	uint32_t pageSize = ketl_get_page_size();
	exeMemory->pageSize = pageSize;
	uint32_t pageSizeLog = -1;
	while (pageSize > 0) {
		pageSize >>= 1;
		++pageSizeLog;
	}
	exeMemory->pageSizeLog = pageSizeLog;
}

void ketlDeinitExecutableMemory(KETLExecutableMemory* exeMemory) {
	KETLObjectPoolIterator pageIterator;
	ketlInitPoolIterator(&pageIterator, &exeMemory->pagePool);

	while (ketlIteratorPoolHasNext(&pageIterator)) {
		KETLExecutableMemoryPage page = *(KETLExecutableMemoryPage*)ketlIteratorPoolGetNext(&pageIterator);
		ketl_deallocate_exe_memory(page.page, page.pageSize);
	}

	ketlDeinitObjectPool(&exeMemory->pagePool);
}

uint8_t* ketlExecutableMemoryAllocate(KETLExecutableMemory* exeMemory, const uint8_t** opcodes, uint64_t blocksize, uint64_t length) {
	KETLExecutableMemoryPage* pCurrentPage = exeMemory->currentPage;
	uint32_t currentOffset = exeMemory->currentOffset;
	KETLExecutableMemoryPage currentPage;
	if (pCurrentPage == NULL || currentOffset + length > pCurrentPage->pageSize) {
		uint32_t pageSize = exeMemory->pageSize;
		uint32_t requestedPageCount = (uint32_t)((length + (pageSize - 1)) >> exeMemory->pageSizeLog);

		currentPage.pageSize = pageSize * requestedPageCount;
		currentPage.page = ketl_allocate_exe_memory(pCurrentPage, currentPage.pageSize);

		pCurrentPage = ketlGetFreeObjectFromPool(&exeMemory->pagePool);
		*pCurrentPage = currentPage;
		exeMemory->currentPage = pCurrentPage;
		currentOffset = 0;
	} else {
		currentPage = *pCurrentPage;
		ketl_unprotect_exe_memory(currentPage.page, currentPage.pageSize);
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
		ketl_protect_exe_memory(currentPage.page, currentPage.pageSize);
	}

	return resultMemory;
}