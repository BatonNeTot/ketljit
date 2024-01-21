//🫖ketl
#ifndef ketl_type_h
#define ketl_type_h

#include "ketl/utils.h"

KETL_DEFINE(ketl_variable_traits) {
	union {
		struct {
			bool isNullable : 1;
			bool isConst : 1;
		} values;
		uint8_t hash : 2;
	};
};

#define KETL_VARIABLE_TRAIT_HASH_COUNT 4

KETL_FORWARD(ketl_type_base);
KETL_FORWARD(ketl_type_primitive);
KETL_FORWARD(ketl_type_function);
KETL_FORWARD(ketl_type_function_class);
KETL_FORWARD(ketl_type_class);
KETL_FORWARD(ketl_type_interface);

KETL_DEFINE(ketl_type_pointer) {
	union {
		ketl_type_base* base;
		ketl_type_primitive* primitive;
		ketl_type_function* function;
		ketl_type_function_class* functionClass;
		ketl_type_class* clazz;
		ketl_type_interface* iterface;
	};
};

uint64_t ketl_type_get_stack_size(ketl_type_pointer type);

uint64_t ketl_type_get_size(ketl_type_pointer type);

#endif // ketl_type_h
