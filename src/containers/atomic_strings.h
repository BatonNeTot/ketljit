//🫖ketl
#ifndef ketl_atomic_strings_h
#define ketl_atomic_strings_h

#include "int_map.h"
#include "ketl/utils.h"

KETL_FORWARD(ketl_atomic_strings_bucket);

KETL_DEFINE(ketl_atomic_strings) {
	ketl_object_pool bucketPool;
	ketl_object_pool stringPool;
	ketl_atomic_strings_bucket** buckets;
	uint64_t size;
	uint64_t capacityIndex;
};

uint64_t ketl_hash_string(const char* str, uint64_t length);

void ketl_atomic_strings_init(ketl_atomic_strings* strings, size_t poolSize);

void ketl_atomic_strings_deinit(ketl_atomic_strings* strings);

const char* ketl_atomic_strings_get(ketl_atomic_strings* strings, const char* key, uint64_t length);

#endif // ketl_atomic_strings_h
