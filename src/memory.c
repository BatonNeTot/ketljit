﻿//🫖ketl
#include "ketl/memory.h"

#include <stdlib.h>

static void* ketl_default_alloc(size_t size, void* userInfo) {
	(void)userInfo;
	return malloc(size);
}

static void* ketl_default_realloc(void* ptr, size_t size, void* userInfo) {
	(void)userInfo;
	return realloc(ptr, size);
}

static void ketl_default_free(void* ptr, void* userInfo) {
	(void)userInfo;
	free(ptr);
}

const ketl_allocator ketl_default_allocator = {
	&ketl_default_alloc,
	&ketl_default_realloc,
	&ketl_default_free,
	NULL
};
