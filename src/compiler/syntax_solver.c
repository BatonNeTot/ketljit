//🫖ketl
#include "compiler/syntax_solver.h"

#include "bnf_parser.h"
#include "syntax_parser.h"

#include "token.h"
#include "lexer.h"
#include "compiler/syntax_node.h"
#include "bnf_node.h"
#include "bnf_scheme.h"
#include "containers/object_pool.h"
#include "containers/stack.h"
#include "assert.h"

#include <string.h>

void ketl_syntax_solver_init(ketl_syntax_solver* syntaxSolver) {
	ketl_object_pool_init(&syntaxSolver->tokenPool, sizeof(ketl_token), 16);
	ketl_object_pool_init(&syntaxSolver->bnfNodePool, sizeof(ketl_bnf_node), 16);
	ketl_stack_init(&syntaxSolver->bnfStateStack, sizeof(ketl_bnf_parser_state), 32);
	syntaxSolver->bnfScheme = ketl_bnf_build_scheme(&syntaxSolver->bnfNodePool);
}

void ketl_syntax_solver_deinit(ketl_syntax_solver* syntaxSolver) {
	ketl_stack_deinit(&syntaxSolver->bnfStateStack);
	ketl_object_pool_deinit(&syntaxSolver->bnfNodePool);
	ketl_object_pool_deinit(&syntaxSolver->tokenPool);
}

ketl_syntax_node* ketl_syntax_solver_solve(const char* source, size_t length, ketl_syntax_solver* syntaxSolver, ketl_object_pool* syntaxNodePool) {
	ketl_stack* bnfStateStack = &syntaxSolver->bnfStateStack;
	ketl_object_pool* tokenPool = &syntaxSolver->tokenPool;

	ketl_lexer lexer;
	ketl_lexer_init(&lexer, source, length, tokenPool);

	if (!ketl_lexer_has_next_token(&lexer)) {
		ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

		node->type = KETL_SYNTAX_NODE_TYPE_BLOCK;
		node->lineInSource = 0;
		node->columnInSource = 0;
		node->value = 0;
		node->firstChild = NULL;
		node->nextSibling = NULL;
		node->length = 0;

		return node;
	}
	ketl_token* firstToken = ketl_lexer_get_next_token(&lexer);
	ketl_token* token = firstToken;

	while (ketl_lexer_has_next_token(&lexer)) {
		token = token->next = ketl_lexer_get_next_token(&lexer);
	}
	token->next = NULL;

	size_t sourceLength = ketl_lexer_current_position(&lexer);

	{
		ketl_bnf_parser_state* initialSolver = ketl_stack_push(bnfStateStack);
		initialSolver->bnfNode = syntaxSolver->bnfScheme;
		initialSolver->token = firstToken;
		initialSolver->tokenOffset = 0;

		initialSolver->parent = NULL;
	}

	ketl_bnf_error_info error;
	{
		error.maxToken = firstToken;
		error.maxTokenOffset = 0;
		error.bnfNode = NULL;
	}

	bool success = ketl_bnf_parse(bnfStateStack, &error);
	if (!success) {
		uint64_t tokenPositionInSource;
		const char* result;
		uint64_t resultLength;
		if (error.maxToken) {
			// has "find instead"
			tokenPositionInSource = error.maxToken->positionInSource + error.maxTokenOffset;
			result = error.maxToken->value + error.maxTokenOffset;
			resultLength = error.maxToken->length;
		} else {
			// unexpected EOF
			// TODO calculate line and column of end
			tokenPositionInSource = length != KETL_NULL_TERMINATED_LENGTH ? length : sourceLength;
			result = "EOF";
			resultLength = 3;
		}

		uint32_t line = 0;
		uint32_t column = 0;
		ketl_count_lines(source, tokenPositionInSource, &line, &column);

		if (error.bnfNode) {
			KETL_ERROR("Expected '%.*s' at '%u:%u', got '%.*s'", 
				error.bnfNode->size, error.bnfNode->value, 
				line, column,
				resultLength, result);
		} else {
			KETL_ERROR("Can't parse bnf", 0);
		}
		return NULL;
	}

	ketl_stack_iterator iterator;
	ketl_stack_iterator_init(&iterator, bnfStateStack);

	ketl_syntax_node* rootSyntaxNode = ketl_syntax_parse(syntaxNodePool, &iterator, source);

	ketl_stack_reset(bnfStateStack);
	ketl_object_pool_reset(tokenPool);
	
	return rootSyntaxNode;
}