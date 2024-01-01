//🫖ketl
#ifndef ketl_type_impl_h
#define ketl_type_impl_h

#include "ketl/type.h"

#include "containers/int_map.h"
#include "ketl/utils.h"

#define KETL_CREATE_TYPE_PTR(base_ptr) ((ketl_type_pointer){{(ketl_type_base*) (base_ptr)}})

typedef uint8_t ketl_type_kind;

#define KETL_TYPE_KIND_PRIMITIVE 0
#define KETL_TYPE_KIND_FUNCTION 1
#define KETL_TYPE_KIND_STRUCT 2
#define KETL_TYPE_KIND_CLASS 3

KETL_DEFINE(ketl_type_base) {
	const char* name;
	ketl_type_kind kind;
};

KETL_DEFINE(ketl_type_primitive) {
	const char* name;
	ketl_type_kind kind;
	uint32_t size;
};

KETL_DEFINE(ketl_type_parameters) {
	uint64_t offset;
	ketl_type_pointer type;
	ketl_variable_traits traits;
};

KETL_DEFINE(ketl_type_function) {
	const char* name;
	ketl_type_kind kind;
	uint32_t parametersCount;
	ketl_type_parameters* parameters;

};

#endif // ketl_type_impl_h
