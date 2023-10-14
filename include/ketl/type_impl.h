//🍲ketl
#ifndef type_impl_h
#define type_impl_h

#include "type.h"

#include "ketl/int_map.h"
#include "ketl/utils.h"

typedef uint8_t KETLTypeKind;

#define KETL_TYPE_KIND_PRIMITIVE 0
#define KETL_TYPE_KIND_FUNCTION 1
#define KETL_TYPE_KIND_STRUCT 2
#define KETL_TYPE_KIND_CLASS 3

KETL_DEFINE(KETLTypeBase) {
	const char* name;
	KETLTypeKind kind;
};

KETL_DEFINE(KETLTypePrimitive) {
	const char* name;
	KETLTypeKind kind;
	uint32_t size;
};

KETL_DEFINE(KETLTypeMembers) {
	uint64_t offset;
	KETLTypePtr type;
	const char* name;
	KETLVariableTraits traits;
};

KETL_DEFINE(KETLTypeFunction) {
	const char* name;
	KETLTypeKind kind;
	uint32_t membersCount;
	KETLTypeMembers* members;
};

#endif /*type_impl_h*/
