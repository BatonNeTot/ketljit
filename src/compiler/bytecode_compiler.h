//🫖ketl
#ifndef ketl_compiler_bytecode_compiler_h
#define ketl_compiler_bytecode_compiler_h

#include "syntax_solver.h"

#include "ketl/utils.h"

KETL_FORWARD(ketl_state);

KETL_DEFINE(ketl_bytecode_compiler) {
	ketl_object_pool syntaxNodePool;
	ketl_syntax_solver syntaxSolver;
};

void ketl_bytecode_solver_init(ketl_bytecode_compiler* compiler);

void ketl_bytecode_solver_deinit(ketl_bytecode_compiler* compiler);

#endif // ketl_compiler_bytecode_compiler_h
