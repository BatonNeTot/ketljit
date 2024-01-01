//🫖ketl
#ifndef ketl_compiler_syntax_solver_h
#define ketl_compiler_syntax_solver_h

#include "ketl/utils.h"
#include "containers/object_pool.h"
#include "containers/stack.h"

KETL_FORWARD(ketl_syntax_node);

KETL_FORWARD(ketl_token);
KETL_FORWARD(ketl_bnf_node);

KETL_DEFINE(ketl_syntax_solver) {
	ketl_object_pool tokenPool;
	ketl_object_pool bnfNodePool;
	ketl_stack bnfStateStack;
	ketl_bnf_node* bnfScheme;
};

void ketl_syntax_solver_init(ketl_syntax_solver* syntaxSolver);

void ketl_syntax_solver_deinit(ketl_syntax_solver* syntaxSolver);

ketl_syntax_node* ketl_syntax_solver_solve(const char* source, size_t length, ketl_syntax_solver* syntaxSolver, ketl_object_pool* syntaxNodePool);

#endif // ketl_compiler_syntax_solver_h
