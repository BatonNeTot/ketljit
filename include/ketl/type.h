//🍲ketl
#ifndef type_h
#define type_h

#include "ketl/utils.h"

typedef uint8_t KETLTraitType;

#define KETL_TRAIT_TYPE_LITERAL 0
#define KETL_TRAIT_TYPE_LVALUE 1
#define KETL_TRAIT_TYPE_RVALUE 2
#define KETL_TRAIT_TYPE_REF 3
#define KETL_TRAIT_TYPE_REF_IN 4

KETL_DEFINE(KETLVariableTraits) {
	KETLTraitType type;
	bool isNullable;
	bool isConst;
};

KETL_FORWARD(KETLTypeBase);
KETL_FORWARD(KETLTypePrimitive);
KETL_FORWARD(KETLTypeFunction);

KETL_DEFINE(KETLTypePtr) {
	union {
		KETLTypeBase* base;
		KETLTypePrimitive* primitive;
		KETLTypeFunction* function;
	};
};

uint64_t getStackTypeSize(KETLVariableTraits traits, KETLTypePtr type);

#endif /*type_h*/
