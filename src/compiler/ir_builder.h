﻿//🍲ketl
#ifndef compiler_ir_builder_h
#define compiler_ir_builder_h

#include "ir_node.h"

#include "ketl/compiler/syntax_node.h"
#include "ketl/type.h"

#include "ketl/utils.h"

#include "ketl/object_pool.h"
#include "ketl/int_map.h"

KETL_FORWARD(KETLState);

KETL_DEFINE(KETLIRVariable) {
	KETLType* type;
	KETLVariableTraits traits;
	KETLIRArgument value;
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

	KETLIntMap operationReferMap;
	KETLIntMap argumentsMap;

	KETLObjectPool castingPool;

	KETLState* state;
};

KETL_DEFINE(KETLIRFunctionWIP) {
	KETLIRBuilder* builder;
	KETLIROperation* rootOperation;

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
	KETLType* type;
};

void ketlInitIRBuilder(KETLIRBuilder* irBuilder, KETLState* state);

void ketlDeinitIRBuilder(KETLIRBuilder* irBuilder);

KETLIRFunction* ketlBuildIR(KETLType* returnType, KETLIRBuilder* irBuilder, KETLSyntaxNode* syntaxNodeRoot);

#endif /*compiler_ir_builder_h*/
