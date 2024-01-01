//🫖ketl
#include "compiler/bytecode_compiler.h"

#include "compiler/syntax_node.h"

void ketl_bytecode_solver_init(ketl_bytecode_compiler* compiler) {
	ketl_object_pool_init(&compiler->syntaxNodePool, sizeof(ketl_syntax_node), 16);
	ketl_syntax_solver_init(&compiler->syntaxSolver);
}

void ketl_bytecode_solver_deinit(ketl_bytecode_compiler* compiler) {
	ketl_syntax_solver_deinit(&compiler->syntaxSolver);
	ketl_object_pool_deinit(&compiler->syntaxNodePool);
}