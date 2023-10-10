//🍲ketl
#ifndef compiler_ir_compiler_h
#define compiler_ir_compiler_h

#include "executable_memory.h"
#include "ketl/int_map.h"
#include "ketl/stack.h"

#include "ketl/utils.h"

KETL_FORWARD(KETLFunction);
KETL_FORWARD(KETLIRFunction);

KETL_DEFINE(KETLIRCompiler) {
	KETLExecutableMemory exeMemory;
    KETLIntMap operationBufferOffsetMap;
    KETLStack jumpList;
};

void ketlIRCompilerInit(KETLIRCompiler* irCompiler);

void ketlIRCompilerDeinit(KETLIRCompiler* irCompiler);

KETLFunction* ketlCompileIR(KETLIRCompiler* irCompiler, KETLIRFunction* irFunction);

#endif /*compiler_ir_compiler_h*/
