//🫖ketl
#ifndef ketl_int_map_h
#define ketl_int_map_h

#include "object_pool.h"
#include "ketl/utils.h"

typedef uint64_t ketl_int_map_key;

KETL_FORWARD(ketl_int_map_bucket_base);

KETL_DEFINE(ketl_int_map) {
	ketl_object_pool bucketPool;
	ketl_int_map_bucket_base** buckets;
	ketl_int_map_bucket_base* freeBuckets;
	uint64_t size;
	uint64_t capacityIndex;
};

void ketl_int_map_init(ketl_int_map* map, size_t objectSize, size_t poolSize);

void ketl_int_map_deinit(ketl_int_map* map);

void* ketl_int_map_get(ketl_int_map* map, ketl_int_map_key key);

// returns true if key is new
bool ketl_int_map_get_or_create(ketl_int_map* map, ketl_int_map_key key, void* ppValue);

inline uint64_t ketl_int_map_get_size(ketl_int_map* map) {
	return map->size;
}

void ketl_int_map_reset(ketl_int_map* map);

KETL_DEFINE(ketl_int_map_iterator) {
	ketl_int_map* map;
	uint64_t currentIndex;
	ketl_int_map_bucket_base* currentBucket;
};

void ketl_int_map_iterator_init(ketl_int_map_iterator* iterator, ketl_int_map* map);

bool ketl_int_map_iterator_has_next(ketl_int_map_iterator* iterator);

void ketl_int_map_iterator_get(ketl_int_map_iterator* iterator, ketl_int_map_key* pKey, void* ppValue);

void ketl_int_map_iterator_next(ketl_int_map_iterator* iterator);

void ketl_int_map_iterator_remove(ketl_int_map_iterator* iterator);

#endif // ketl_int_map_h
