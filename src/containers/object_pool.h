//🫖ketl
#ifndef ketl_object_pool_h
#define ketl_object_pool_h

#include "ketl/utils.h"

KETL_FORWARD(ketl_object_pool_bucket_base);

KETL_DEFINE(ketl_object_pool) {
	size_t objectSize;
	size_t poolSize;
	ketl_object_pool_bucket_base* firstPool;
	ketl_object_pool_bucket_base* lastPool;
	size_t occupiedObjects;
};

void ketl_object_pool_init(ketl_object_pool* pool, size_t objectSize, size_t poolSize);

void ketl_object_pool_deinit(ketl_object_pool* pool);

void* ketl_object_pool_get(ketl_object_pool* pool);

void* ketl_object_pool_get_array(ketl_object_pool* pool, uint64_t count);

uint64_t ketl_object_pool_get_used_count(ketl_object_pool* pool);

void ketl_object_pool_reset(ketl_object_pool* pool);

KETL_DEFINE(ketl_object_pool_iterator) {
	ketl_object_pool* pool;
	ketl_object_pool_bucket_base* currentPool;
	size_t nextObjectIndex;
};

void ketl_object_pool_iterator_init(ketl_object_pool_iterator* iterator, ketl_object_pool* pool);

bool ketl_object_pool_iterator_has_next(ketl_object_pool_iterator* iterator);

void* ketl_object_pool_iterator_get_next(ketl_object_pool_iterator* iterator);

#endif // ketl_object_pool_h
