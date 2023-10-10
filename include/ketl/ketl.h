﻿//🍲ketl
#ifndef ketl_h
#define ketl_h

#include "compiler/ir_compiler.h"
#include "compiler/compiler.h"
#include "ketl/atomic_strings.h"
#include "ketl/object_pool.h"
#include "ketl/int_map.h"
#include "type.h"
#include "ketl/operators.h"
#include "ketl/utils.h"

KETL_FORWARD(KETLType);

KETL_DEFINE(KETLNamespace) {
	KETLIntMap variables;
	KETLIntMap namespaces;
	KETLIntMap types;
};

KETL_DEFINE(KETLUnaryOperator) {
	KETLIROperationCode code;
	KETLVariableTraits inputTraits;
	KETLVariableTraits outputTraits;
	KETLType* inputType;
	KETLType* outputType;
	KETLUnaryOperator* next;
};

KETL_DEFINE(KETLBinaryOperator) {
	KETLIROperationCode code;
	KETLVariableTraits lhsTraits;
	KETLVariableTraits rhsTraits;
	KETLVariableTraits outputTraits;
	KETLType* lhsType;
	KETLType* rhsType;
	KETLType* outputType;
	KETLBinaryOperator* next;
};

KETL_DEFINE(KETLCastOperator) {
	KETLIROperationCode code;
	bool implicit;
	KETLVariableTraits inputTraits;
	KETLVariableTraits outputTraits;
	KETLType* outputType;
	KETLCastOperator* next;
};

KETL_DEFINE(KETLState) {
	KETLAtomicStrings strings;
	KETLCompiler compiler;
	KETLIRCompiler irCompiler;
	KETLNamespace globalNamespace;
	struct {
		KETLType* bool_t;
		KETLType* i8_t;
		KETLType* i16_t;
		KETLType* i32_t;
		KETLType* i64_t;
		KETLType* u8_t;
		KETLType* u16_t;
		KETLType* u32_t;
		KETLType* u64_t;
		KETLType* f32_t;
		KETLType* f64_t;
	} primitives;

	KETLObjectPool unaryOperatorsPool;
	KETLObjectPool binaryOperatorsPool;
	KETLObjectPool castOperatorsPool;
	KETLIntMap unaryOperators;
	KETLIntMap binaryOperators;
	KETLIntMap castOperators;

	KETLObjectPool undefVarPool;
	KETLObjectPool variablesPool;
};

void ketlInitState(KETLState* state);

void ketlDeinitState(KETLState* state);

void ketlDefineVariable(KETLState* state, const char* name, KETLType* type, void* pointer);

#endif /*ketl_h*/
