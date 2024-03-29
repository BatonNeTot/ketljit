﻿//🫖ketl
#ifndef ketl_type_impl_h
#define ketl_type_impl_h

#include "ketl/type.h"

#include "compiler/ir_node.h"

#include "containers/int_map.h"
#include "ketl/utils.h"

#define KETL_CREATE_TYPE_PTR(base_ptr) ((ketl_type_pointer){.base = (ketl_type_base*) (base_ptr)})

typedef uint8_t ketl_type_kind;

#define KETL_TYPE_KIND_META 0
#define KETL_TYPE_KIND_PRIMITIVE 1
#define KETL_TYPE_KIND_FUNCTION 2
#define KETL_TYPE_KIND_FUNCTION_CLASS 3
#define KETL_TYPE_KIND_CLASS 4
#define KETL_TYPE_KIND_INTERFACE 5

KETL_DEFINE(ketl_type_base) {
	const char* name;
	ketl_type_kind kind;
};

KETL_DEFINE(ketl_type_meta) {
	const char* name;
	ketl_type_kind kind;
	uint32_t size;
};

KETL_DEFINE(ketl_type_primitive) {
	const char* name;
	ketl_type_kind kind;
	uint32_t size;
};

KETL_DEFINE(ketl_type_parameters) {
	ketl_type_pointer type;
	ketl_variable_traits traits;
	uint32_t offset;
};

KETL_DEFINE(ketl_type_function) {
	const char* name;
	ketl_type_kind kind;
	uint16_t parameterCount;
	ketl_type_parameters* parameters;

};

KETL_DEFINE(ketl_type_field) {
	const char* name;
	uint32_t offset;
	ketl_variable_traits traits;
	ketl_type_pointer type;
};

KETL_DEFINE(ketl_type_function_class) {
	const char* name; // must be copy of functionType->name
	ketl_type_kind kind;
	uint16_t staticFieldCount;
	uint16_t fieldCount;
	uint32_t size;
	ketl_type_function* functionType;
	ketl_type_field* staticFields;
	ketl_type_field* fields;

};

KETL_DEFINE(ketl_type_interface) {
	const char* name;
	ketl_type_kind kind;
	uint8_t interfaceCount;
	uint16_t staticFieldCount;
	ketl_type_pointer* interfaces;
	ketl_type_field* staticFields;
};

KETL_DEFINE(ketl_type_class) {
	const char* name;
	ketl_type_kind kind;
	uint8_t interfaceCount;
	uint16_t staticFieldCount;
	uint16_t fieldCount;
	uint16_t methodCount;
	uint32_t size;
	ketl_type_pointer ancestor;
	ketl_type_pointer* interfaces;
	ketl_type_field* staticFields;
	ketl_type_field* fields;
	ketl_type_function** methods;
};

KETL_FORWARD(ketl_state);

ketl_ir_argument_type get_ir_argument_type(ketl_state* state, ketl_type_pointer type);

#endif // ketl_type_impl_h
