//🫖ketl
#include "compiler/compiler.h"

void ketl_compiler_init(ketl_compiler* compiler, ketl_state* state) {
	ketl_bytecode_solver_init(&compiler->bytecodeCompiler);
	ketl_ir_builder_init(&compiler->irBuilder, state);
}

void ketl_compiler_deinit(ketl_compiler* compiler) {
	ketl_ir_builder_deinit(&compiler->irBuilder);
	ketl_bytecode_solver_deinit(&compiler->bytecodeCompiler);
}