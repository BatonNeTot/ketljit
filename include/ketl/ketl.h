//🍲ketl
#ifndef ketl_h
#define ketl_h

#include "function.h"
#include "type_impl.h"
#include "ketl/operators.h"

#include "compiler/ir_compiler.h"
#include "compiler/compiler.h"

#include "ketl/atomic_strings.h"
#include "ketl/object_pool.h"
#include "ketl/int_map.h"
#include "ketl/utils.h"

KETL_DEFINE(KETLNamespace) {
	KETLIntMap variables;
	KETLIntMap namespaces;
	KETLIntMap types;
};

KETL_DEFINE(KETLUnaryOperator) {
	KETLIROperationCode code;
	KETLVariableTraits inputTraits;
	KETLVariableTraits outputTraits;
	KETLTypePtr inputType;
	KETLTypePtr outputType;
	KETLUnaryOperator* next;
};

KETL_DEFINE(KETLBinaryOperator) {
	KETLIROperationCode code;
	KETLVariableTraits lhsTraits;
	KETLVariableTraits rhsTraits;
	KETLVariableTraits outputTraits;
	KETLTypePtr lhsType;
	KETLTypePtr rhsType;
	KETLTypePtr outputType;
	KETLBinaryOperator* next;
};

KETL_DEFINE(KETLCastOperator) {
	KETLIROperationCode code;
	bool implicit;
	KETLVariableTraits inputTraits;
	KETLVariableTraits outputTraits;
	KETLTypePtr outputType;
	KETLCastOperator* next;
};

KETL_DEFINE(KETLState) {
	KETLAtomicStrings strings;
	KETLCompiler compiler;
	KETLIRCompiler irCompiler;
	KETLNamespace globalNamespace;
	struct {
		KETLTypePrimitive void_t;
		KETLTypePrimitive bool_t;
		KETLTypePrimitive i8_t;
		KETLTypePrimitive i16_t;
		KETLTypePrimitive i32_t;
		KETLTypePrimitive i64_t;
		KETLTypePrimitive u8_t;
		KETLTypePrimitive u16_t;
		KETLTypePrimitive u32_t;
		KETLTypePrimitive u64_t;
		KETLTypePrimitive f32_t;
		KETLTypePrimitive f64_t;
	} primitives;

	KETLObjectPool unaryOperatorsPool;
	KETLObjectPool binaryOperatorsPool;
	KETLObjectPool castOperatorsPool;
	KETLIntMap unaryOperators;
	KETLIntMap binaryOperators;
	KETLIntMap castOperators;

	KETLObjectPool undefVarPool;
	KETLObjectPool variablesPool;

	KETLObjectPool typeParametersPool;
	KETLObjectPool typeFunctionsPool;
	KETLIntMap typeFunctionSearchMap;
};

void ketlInitState(KETLState* state);

void ketlDeinitState(KETLState* state);

void ketl_state_define_external_variable(KETLState* state, const char* name, KETLTypePtr type, void* pointer);

void* ketl_state_define_internal_variable(KETLState* state, const char* name, KETLTypePtr type);

KETLTypePtr getFunctionType(KETLState* state, KETLTypeParameters* parameters, uint64_t parametersCount);

KETLFunction* ketlCompileFunction(KETLState* state, const char* source, KETLParameter* parameters, uint64_t parametersCount);

int64_t ketl_state_eval_local(KETLState* state, const char* source);

int64_t ketl_state_eval(KETLState* state, const char* source);

#endif /*ketl_h*/
