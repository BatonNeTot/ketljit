//🫖ketl
#include "object_pool.h"

#include <stdlib.h>

KETL_DEFINE(ketl_object_pool_bucket_base) {
	ketl_object_pool_bucket_base* next;
};

static inline ketl_object_pool_bucket_base* createObjectPoolBase(ketl_object_pool* pool) {
	ketl_object_pool_bucket_base* poolBase = malloc(sizeof(ketl_object_pool_bucket_base) + pool->objectSize * pool->poolSize);
	poolBase->next = NULL;
	return poolBase;
}

void ketl_object_pool_init(ketl_object_pool* pool, size_t objectSize, size_t poolSize) {
	// TODO align by adjusting poolSize
	pool->objectSize = objectSize;
	pool->poolSize = poolSize;

	pool->firstPool = pool->lastPool = createObjectPoolBase(pool);
	pool->occupiedObjects = 0;
}

void ketl_object_pool_deinit(ketl_object_pool* pool) {
	ketl_object_pool_bucket_base* it = pool->firstPool;
	while (it) {
		ketl_object_pool_bucket_base* next = it->next;
		free(it);
		it = next;
	}
}

void* ketl_object_pool_get(ketl_object_pool* pool) {
	if (pool->occupiedObjects >= pool->poolSize) {
		if (pool->lastPool->next) {
			pool->lastPool = pool->lastPool->next;
		}
		else {
			ketl_object_pool_bucket_base* poolBase = createObjectPoolBase(pool);
			pool->lastPool->next = poolBase;
			pool->lastPool = poolBase;
		}
		pool->occupiedObjects = 0;
	}
	return ((char*)(pool->lastPool + 1)) + pool->objectSize * pool->occupiedObjects++;
}

void* ketl_object_pool_get_array(ketl_object_pool* pool, uint64_t count) {
	if (count == 0 || count > pool->poolSize) {
		return NULL;
	}
	if (pool->occupiedObjects + count > pool->poolSize) {
		if (pool->lastPool->next) {
			pool->lastPool = pool->lastPool->next;
		}
		else {
			ketl_object_pool_bucket_base* poolBase = createObjectPoolBase(pool);
			pool->lastPool->next = poolBase;
			pool->lastPool = poolBase;
		}
		pool->occupiedObjects = 0;
	}
	void* value = ((char*)(pool->lastPool + 1)) + pool->objectSize * pool->occupiedObjects;
	pool->occupiedObjects += count;
	return value;
}

uint64_t ketl_object_pool_get_used_count(ketl_object_pool* pool) {
	ketl_object_pool_bucket_base* baseIterator = pool->firstPool;
	ketl_object_pool_bucket_base* lastPoolNext = pool->lastPool->next;
	uint64_t poolSize = pool->poolSize;
	uint64_t usedCount = 0;
	KETL_FOREVER {
		ketl_object_pool_bucket_base* next = baseIterator->next;
		if (next == lastPoolNext) {
			break;
		}
		usedCount += poolSize;
		baseIterator = next;
	}
	return usedCount + pool->occupiedObjects;
}

void ketl_object_pool_reset(ketl_object_pool* pool) {
	pool->lastPool = pool->firstPool;
	pool->occupiedObjects = 0;
}

void ketl_object_pool_iterator_init(ketl_object_pool_iterator* iterator, ketl_object_pool* pool) {
	iterator->pool = pool;
	iterator->currentPool = pool->firstPool;
	iterator->nextObjectIndex = 0;
}

bool ketl_object_pool_iterator_has_next(ketl_object_pool_iterator* iterator) {
	return iterator->currentPool != NULL && (iterator->currentPool != iterator->pool->lastPool || iterator->nextObjectIndex < iterator->pool->occupiedObjects);
}

void* ketl_object_pool_iterator_get_next(ketl_object_pool_iterator* iterator) {
	if (iterator->nextObjectIndex >= iterator->pool->poolSize) {
		iterator->currentPool = iterator->currentPool->next;
		iterator->nextObjectIndex = 0;
	}
	return ((char*)(iterator->currentPool + 1)) + iterator->pool->objectSize * iterator->nextObjectIndex++;
}