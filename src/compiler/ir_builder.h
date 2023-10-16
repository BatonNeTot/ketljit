//🍲ketl
#ifndef compiler_ir_builder_h
#define compiler_ir_builder_h

#include "ir_node.h"

#include "ketl/compiler/syntax_node.h"
#include "ketl/type.h"
#include "ketl/function.h"

#include "ketl/object_pool.h"
#include "ketl/int_map.h"
#include "ketl/stack.h"

#include "ketl/utils.h"

KETL_FORWARD(KETLState);

KETL_DEFINE(KETLIRVariable) {
	KETLTypePtr type;
	KETLVariableTraits traits;
	KETLIRArgument value;
};

KETL_DEFINE(IRUndefinedValue) {
	KETLIRVariable* variable;
	uint64_t scopeIndex;
	IRUndefinedValue* next;
};

KETL_DEFINE(KETLIRScopedVariable) {
	KETLIRVariable variable;
	KETLIRScopedVariable* parent;
	KETLIRScopedVariable* nextSibling;
	KETLIRScopedVariable* firstChild;
};

KETL_DEFINE(KETLIRBuilder) {
	KETLIntMap variablesMap;

	KETLObjectPool scopedVariablesPool;
	KETLObjectPool variablesPool;
	KETLObjectPool operationsPool;
	KETLObjectPool udelegatePool;
	KETLObjectPool uvaluePool;

	KETLObjectPool argumentPointersPool;
	KETLObjectPool argumentsPool;

	KETLObjectPool castingPool;

	KETLIntMap operationReferMap;
	KETLIntMap argumentsMap;
	KETLStack extraNextStack;

	KETLState* state;
};

KETL_DEFINE(IROperationRange) {
	KETLIROperation* root;
	KETLIROperation* next;
};

KETL_DEFINE(KETLIRFunctionWIP) {
	KETLIRBuilder* builder;
	IROperationRange operationRange;

	union {
		struct {
			KETLIRScopedVariable* stackRoot;
			KETLIRScopedVariable* currentStack;
		};
		struct {
			KETLIRScopedVariable* tempVariables;
			KETLIRScopedVariable* localVariables;
		};
	};

	uint64_t scopeIndex;
};

KETL_DEFINE(KETLIRFunctionDefinition) {
	KETLIRFunction* function;
	KETLTypePtr type;
};

void ketlInitIRBuilder(KETLIRBuilder* irBuilder, KETLState* state);

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder);

KETLIRFunctionDefinition ketlBuildIR(KETLTypePtr returnType, KETLIRBuilder* irBuilder, KETLSyntaxNode* syntaxNodeRoot, KETLParameter* parameters, uint64_t parametersCount);

#endif /*compiler_ir_builder_h*/
