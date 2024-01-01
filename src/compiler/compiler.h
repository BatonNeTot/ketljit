//🫖ketl
#ifndef ketl_compiler_compiler_h
#define ketl_compiler_compiler_h

#include "compiler/ir_builder.h"
#include "compiler/bytecode_compiler.h"

#include "ketl/utils.h"

KETL_FORWARD(ketl_state);

KETL_DEFINE(ketl_compiler) {
	ketl_bytecode_compiler bytecodeCompiler;
	ketl_ir_builder irBuilder;
};

void ketl_compiler_init(ketl_compiler* compiler, ketl_state* state);

void ketl_compiler_deinit(ketl_compiler* compiler);

#endif // ketl_compiler_compiler_h
