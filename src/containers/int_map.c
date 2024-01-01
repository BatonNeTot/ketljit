//🫖ketl
#include "int_map.h"

#include "common.h"

#include <stdlib.h>
#include <string.h>

KETL_DEFINE(ketl_int_map_bucket_base) {
	ketl_int_map_key key;
	ketl_int_map_bucket_base* next;
};

void ketl_int_map_init(ketl_int_map* map, size_t objectSize, size_t poolSize) {
	// TODO align
	const size_t bucketSize = sizeof(ketl_int_map_bucket_base) + objectSize;
	ketl_object_pool_init(&map->bucketPool, bucketSize, poolSize);
	map->capacityIndex = 0;
	uint64_t capacity = ketl_prime_capacities[0];
	map->size = 0;
	uint64_t arraySize = sizeof(ketl_int_map_bucket_base*) * capacity;
	ketl_int_map_bucket_base** buckets = map->buckets = malloc(arraySize);
	// TODO use custom memset
	memset(buckets, 0, arraySize);
}

void ketl_int_map_deinit(ketl_int_map* map) {
	ketl_object_pool_deinit(&map->bucketPool);
	free(map->buckets);
}

uint64_t ketl_int_map_get_size(ketl_int_map* map);

void* ketl_int_map_get(ketl_int_map* map, ketl_int_map_key key) {
	uint64_t capacity = ketl_prime_capacities[map->capacityIndex];
	ketl_int_map_bucket_base** buckets = map->buckets;
	uint64_t index = key % capacity;
	ketl_int_map_bucket_base* bucket = buckets[index];

	while (bucket) {
		if (bucket->key == key) {
			return bucket + 1;
		}

		bucket = bucket->next;
	}
	return NULL;
}

bool ketl_int_map_get_or_create(ketl_int_map* map, ketl_int_map_key key, void* ppValue) {
	uint64_t capacity = ketl_prime_capacities[map->capacityIndex];
	ketl_int_map_bucket_base** buckets = map->buckets;
	uint64_t index = key % capacity;
	ketl_int_map_bucket_base* bucket = buckets[index];

	while (bucket) {
		if (bucket->key == key) {
			*(void**)(ppValue) = bucket + 1;
			return false;
		}

		bucket = bucket->next;
	}

	uint64_t size = ++map->size;
	if (size > capacity) {
		uint64_t newCapacityIndex = map->capacityIndex + 1;
		if (KETL_PRIME_CAPACITIES_TOTAL <= newCapacityIndex) {
			// TODO error
			return false;
		}
		uint64_t newCapacity = ketl_prime_capacities[map->capacityIndex = newCapacityIndex];
		uint64_t arraySize = sizeof(ketl_int_map_bucket_base*) * newCapacity;
		ketl_int_map_bucket_base** newBuckets = map->buckets = malloc(arraySize);
		// TODO use custom memset
		memset(newBuckets, 0, arraySize);
		for (uint64_t i = 0; i < capacity; ++i) {
			bucket = buckets[i];
			while (bucket) {
				ketl_int_map_bucket_base* next = bucket->next;
				uint64_t newIndex = bucket->key % newCapacity;
				bucket->next = newBuckets[newIndex];
				newBuckets[newIndex] = bucket;
				bucket = next;
			}
		}
		free(buckets);
		index = key % newCapacity;
		buckets = newBuckets;
	}

	bucket = ketl_object_pool_get(&map->bucketPool);

	bucket->key = key;
	bucket->next = buckets[index];
	buckets[index] = bucket;

	*(void**)(ppValue) = bucket + 1;
	return true;
}

void ketl_int_map_reset(ketl_int_map* map) {
	ketl_object_pool_reset(&map->bucketPool);
	map->size = 0;
	uint64_t capacity = ketl_prime_capacities[map->capacityIndex];
	uint64_t arraySize = sizeof(ketl_int_map_bucket_base*) * capacity;
	// TODO use custom memset
	memset(map->buckets, 0, arraySize);
}


void ketl_int_map_iterator_init(ketl_int_map_iterator* iterator, ketl_int_map* map) {
	iterator->map = map;
	uint64_t i = 0;
	uint64_t capacity = ketl_prime_capacities[map->capacityIndex];
	ketl_int_map_bucket_base** buckets = map->buckets;
	KETL_FOREVER{
		if (i >= capacity) {
			iterator->currentBucket = NULL;
			return;
		}

		if (buckets[i] != NULL) {
			iterator->currentIndex = i;
			iterator->currentBucket = buckets[i];
			return;
		}

		++i;
	}
}

bool ketl_int_map_iterator_has_next(ketl_int_map_iterator* iterator) {
	return iterator->currentBucket != NULL;
}

void ketl_int_map_iterator_get(ketl_int_map_iterator* iterator, ketl_int_map_key* pKey, void* ppValue) {
	*pKey = iterator->currentBucket->key;
	*(void**)(ppValue) = iterator->currentBucket + 1;
}

void ketl_int_map_iterator_next(ketl_int_map_iterator* iterator) {
	ketl_int_map_bucket_base* nextBucket = iterator->currentBucket->next;
	if (nextBucket) {
		iterator->currentBucket = nextBucket;
		return;
	}

	uint64_t i = iterator->currentIndex + 1;
	uint64_t capacity = ketl_prime_capacities[iterator->map->capacityIndex];
	ketl_int_map_bucket_base** buckets = iterator->map->buckets;
	KETL_FOREVER{
		if (i >= capacity) {
			iterator->currentBucket = NULL;
			return;
		}

		if (buckets[i] != NULL) {
			iterator->currentIndex = i;
			iterator->currentBucket = buckets[i];
			return;
		}

		++i;
	}
}

void ketl_int_map_iterator_remove(ketl_int_map_iterator* iterator) {
	--iterator->map->size;
	uint64_t i = iterator->currentIndex;
	ketl_int_map_bucket_base** buckets = iterator->map->buckets;
	
	ketl_int_map_bucket_base* parent = buckets[i];
	ketl_int_map_bucket_base* currentBucket = iterator->currentBucket;
	if (parent == currentBucket) {
		buckets[i] = currentBucket = currentBucket->next;
	}
	else {
		while (parent->next != currentBucket) {
			parent = parent->next;
		}
		parent->next = currentBucket = currentBucket->next;
	}

	if (currentBucket != NULL) {
		iterator->currentBucket = currentBucket;
		return;
	}

	++i;
	uint64_t capacity = ketl_prime_capacities[iterator->map->capacityIndex];
	KETL_FOREVER{
		if (i >= capacity) {
			iterator->currentBucket = NULL;
			return;
		}

		if (buckets[i] != NULL) {
			iterator->currentIndex = i;
			iterator->currentBucket = buckets[i];
			return;
		}

		++i;
	}
}