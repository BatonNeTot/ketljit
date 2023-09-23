//🍲ketl
#ifndef compiler_ir_compiler_h
#define compiler_ir_compiler_h

#include "ketl/utils.h"

KETL_FORWARD(KETLFunction);
KETL_FORWARD(KETLIRFunction);
KETL_FORWARD(KETLExecutableMemory);

KETLFunction* ketlCompileIR(KETLExecutableMemory* exeMemory, KETLIRFunction* irFunction);

#endif /*compiler_ir_compiler_h*/
