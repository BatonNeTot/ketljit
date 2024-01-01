//🫖ketl
#ifndef ketl_type_h
#define ketl_type_h

#include "ketl/utils.h"

typedef uint8_t ketl_trait_type;

#define KETL_TRAIT_TYPE_LITERAL 0
#define KETL_TRAIT_TYPE_LVALUE 1
#define KETL_TRAIT_TYPE_RVALUE 2
#define KETL_TRAIT_TYPE_REF 3
#define KETL_TRAIT_TYPE_REF_IN 4

KETL_DEFINE(ketl_variable_traits) {
	union {
		struct {
			bool isNullable : 1;
			bool isConst : 1;
			ketl_trait_type type : 3;
		} values;
		uint8_t hash : 5;
	};
};

#define KETL_VARIABLE_TRAIT_HASH_COUNT 20

KETL_FORWARD(ketl_type_base);
KETL_FORWARD(ketl_type_primitive);
KETL_FORWARD(ketl_type_function);

KETL_DEFINE(ketl_type_pointer) {
	union {
		ketl_type_base* base;
		ketl_type_primitive* primitive;
		ketl_type_function* function;
	};
};

uint64_t ketl_type_get_stack_size(ketl_variable_traits traits, ketl_type_pointer type);

#endif // ketl_type_h
