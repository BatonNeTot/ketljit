//🫖ketl
#include "syntax_parser.h"

#include "token.h"
#include "compiler/syntax_node.h"
#include "bnf_node.h"
#include "bnf_parser.h"
#include "containers/stack.h"
#include "containers/object_pool.h"

void ketl_count_lines(const char* source, uint64_t length, uint32_t* pLine, uint32_t* pColumn) {
	uint64_t line = 0;
	uint64_t column = 0;

	for (uint64_t i = 0u; i < length; ++i) {
		if (source[i] == '\n') {
			++line;
			column = 0;
		} else {
			++column;
		}
	}

	*pLine = line;
	*pColumn = column;
}

static ketl_syntax_node_type decideOperatorSyntaxType(const char* value, uint32_t length) {
	switch (length) {
	case 1: {
		switch (*value) {
		case '+':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PLUS;
		case '-':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_MINUS;
		case '*':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_PROD;
		case '/':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_DIV;
		}
		break;
	}
	case 2: {
		switch (*value) {
		case '=':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_EQUAL;
		case '!':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_UNEQUAL;
		case ':':
			return KETL_SYNTAX_NODE_TYPE_OPERATOR_BI_ASSIGN;
		}
		break;
	}
	}
	KETL_DEBUGBREAK();
	return 0;
}

static uint32_t calculateNodeLength(ketl_syntax_node* node) {
	uint32_t length = 0;

	while (node) {
		++length;
		node = node->nextSibling;
	}

	return length;
}

ketl_syntax_node* ketl_syntax_parse(ketl_object_pool* syntaxNodePool, ketl_stack_iterator* bnfStackIterator, const char* source) {
	ketl_bnf_parser_state* state = ketl_stack_iterator_get_next(bnfStackIterator);

	switch (state->bnfNode->builder) {
	case KETL_SYNTAX_BUILDER_TYPE_BLOCK: {
		ketl_syntax_node* first = NULL;
		ketl_syntax_node* last = NULL;

		KETL_FOREVER{ 
			if (!ketl_stack_iterator_has_next(bnfStackIterator)) {
				break;
			}

			KETL_ITERATOR_STACK_PEEK(ketl_bnf_parser_state*, next, *bnfStackIterator);

			if (next->parent != state) {
				break;
			}

			ketl_stack_iterator_skip_next(bnfStackIterator); // ref
			ketl_syntax_node* command = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);

			if (command == NULL) {
				continue;
			}

			if (first == NULL) {
				first = command;
				last = command;
			}
			else {
				last->nextSibling = command;
				last = command;
			}
		}

		if (last != NULL) {
			last->nextSibling = NULL;
		}
		return first;
	}
	case KETL_SYNTAX_BUILDER_TYPE_COMMAND: {
		state = ketl_stack_iterator_get_next(bnfStackIterator);
		switch (state->bnfNode->type) {
		case KETL_BNF_NODE_TYPE_REF:
			return ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
		case KETL_BNF_NODE_TYPE_CONCAT:
			state = ketl_stack_iterator_get_next(bnfStackIterator); // '{' or optional
			if (state->bnfNode->type == KETL_BNF_NODE_TYPE_OPTIONAL) {
				// expression
				KETL_ITERATOR_STACK_PEEK(ketl_bnf_parser_state*, next, *bnfStackIterator);

				ketl_syntax_node* node = NULL;
				if (next->parent == state) {
					ketl_stack_iterator_skip_next(bnfStackIterator); // ref
					node = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
				}

				ketl_stack_iterator_skip_next(bnfStackIterator); // ;
				return node;
			}
			else {
				// block
				ketl_stack_iterator_skip_next(bnfStackIterator); // ref
				ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

				node->type = KETL_SYNTAX_NODE_TYPE_BLOCK;
				ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
					&node->lineInSource, &node->columnInSource);
				node->value = state->token->value + state->tokenOffset;
				node->firstChild = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
				node->length = calculateNodeLength(node->firstChild);

				ketl_stack_iterator_skip_next(bnfStackIterator); // }

				return node;
			}
		default:
			KETL_DEBUGBREAK();
		}
		KETL_DEBUGBREAK();
		break;
	}
	case KETL_SYNTAX_BUILDER_TYPE_TYPE:
		state = ketl_stack_iterator_get_next(bnfStackIterator); // id or cocncat
		if (state->bnfNode->type == KETL_BNF_NODE_TYPE_ID) {
			ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_ID;
			ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

			return node;
		}
		KETL_DEBUGBREAK();
		break;
	case KETL_SYNTAX_BUILDER_TYPE_PRIMARY_EXPRESSION:
		state = ketl_stack_iterator_get_next(bnfStackIterator);
		switch (state->bnfNode->type) {
		case KETL_BNF_NODE_TYPE_ID: {
			ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_ID;
			ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

			return node;
		}
		case KETL_BNF_NODE_TYPE_NUMBER: {
			ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

			node->type = KETL_SYNTAX_NODE_TYPE_NUMBER;
			ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->value = state->token->value + state->tokenOffset;
			node->length = state->token->length - state->tokenOffset;

			return node;
		}
		case KETL_BNF_NODE_TYPE_CONCAT: {
			ketl_stack_iterator_skip_next(bnfStackIterator); // (
			ketl_stack_iterator_skip_next(bnfStackIterator); // ref
			ketl_syntax_node* node = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
			ketl_stack_iterator_skip_next(bnfStackIterator); // )
			return node;
		}
		default:
			KETL_DEBUGBREAK();
		}
		break;
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_1: {
		// LEFT TO RIGHT
		ketl_stack_iterator_skip_next(bnfStackIterator); // ref
		ketl_syntax_node* left = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
		ketl_bnf_parser_state* state = ketl_stack_iterator_get_next(bnfStackIterator); // repeat
		KETL_ITERATOR_STACK_PEEK(ketl_bnf_parser_state*, next, *bnfStackIterator);

		if (next->parent == state) {
			KETL_DEBUGBREAK();
		}

		return left;
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_2: {
		// LEFT TO RIGHT
		ketl_stack_iterator_skip_next(bnfStackIterator); // ref
		ketl_syntax_node* caller = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
		ketl_bnf_parser_state* state = ketl_stack_iterator_get_next(bnfStackIterator); // repeat
		KETL_ITERATOR_STACK_PEEK(ketl_bnf_parser_state*, next, *bnfStackIterator);

		if (next->parent == state) {
			KETL_DEBUGBREAK();
		}

		return caller;
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_3: 
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_4: {
		// LEFT TO RIGHT
		ketl_stack_iterator_skip_next(bnfStackIterator); // ref
		ketl_syntax_node* left = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);

		ketl_bnf_parser_state* repeat = ketl_stack_iterator_get_next(bnfStackIterator); // repeat
		KETL_FOREVER {
			KETL_ITERATOR_STACK_PEEK(ketl_bnf_parser_state*, next, *bnfStackIterator);

			if (next->parent != repeat) {
				return left;
			}

			ketl_stack_iterator_skip_next(bnfStackIterator); // concat
			ketl_stack_iterator_skip_next(bnfStackIterator); // or

			ketl_bnf_parser_state* op = ketl_stack_iterator_get_next(bnfStackIterator);

			ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

			node->type = decideOperatorSyntaxType(op->token->value + op->tokenOffset, op->token->length - op->tokenOffset);
			ketl_count_lines(source, op->token->positionInSource + op->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->length = 2;
			node->firstChild = left;

			ketl_stack_iterator_skip_next(bnfStackIterator); // ref
			ketl_syntax_node* right = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);

			left->nextSibling = right;
			right->nextSibling = NULL;

			left = node;
		}
	}
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_5:
	case KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_6: {
		// RIGHT TO LEFT
		ketl_stack_iterator_skip_next(bnfStackIterator); // ref
		ketl_syntax_node* root = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
		ketl_syntax_node* left = NULL;

		ketl_bnf_parser_state* repeat = ketl_stack_iterator_get_next(bnfStackIterator); // repeat
		KETL_FOREVER{
			KETL_ITERATOR_STACK_PEEK(ketl_bnf_parser_state*, next, *bnfStackIterator);

			if (next->parent != repeat) {
				return root;
			}

			ketl_stack_iterator_skip_next(bnfStackIterator); // concat
			ketl_stack_iterator_skip_next(bnfStackIterator); // or

			ketl_bnf_parser_state* op = ketl_stack_iterator_get_next(bnfStackIterator);

			ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

			node->type = decideOperatorSyntaxType(op->token->value + op->tokenOffset, op->token->length - op->tokenOffset);
			ketl_count_lines(source, op->token->positionInSource + op->tokenOffset, 
				&node->lineInSource, &node->columnInSource);
			node->length = 2;
			if (left != NULL) {
				node->firstChild = left->nextSibling;
				left->nextSibling = node;
				left = node->firstChild;
				node->nextSibling = NULL;
			}
			else {
				node->firstChild = left = root;
				root = node;
			}

			ketl_stack_iterator_skip_next(bnfStackIterator); // ref
			ketl_syntax_node* right = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);

			left->nextSibling = right;
			right->nextSibling = NULL;
		}
	}
	case KETL_SYNTAX_BUILDER_TYPE_DEFINE_WITH_ASSIGNMENT: {
		ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

		ketl_stack_iterator_skip_next(bnfStackIterator); // or
		state = ketl_stack_iterator_get_next(bnfStackIterator); // var or type
		ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
			&node->lineInSource, &node->columnInSource);

		ketl_syntax_node* typeNode = NULL;

		if (state->bnfNode->type == KETL_BNF_NODE_TYPE_CONSTANT) {
			node->length = 2;
			node->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR;
		}
		else {
			node->length = 3;
			typeNode = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
			node->firstChild = typeNode;
			node->type = KETL_SYNTAX_NODE_TYPE_DEFINE_VAR_OF_TYPE;
		}

		state = ketl_stack_iterator_get_next(bnfStackIterator); // id


		ketl_syntax_node* idNode = ketl_object_pool_get(syntaxNodePool);

		idNode->type = KETL_SYNTAX_NODE_TYPE_ID;
		ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
			&idNode->lineInSource, &idNode->columnInSource);
		idNode->length = state->token->length - state->tokenOffset;
		idNode->value = state->token->value + state->tokenOffset;

		if (typeNode) {
			typeNode->nextSibling = idNode;
		}
		else {
			node->firstChild = idNode;
		}

		ketl_stack_iterator_skip_next(bnfStackIterator); // :=
		ketl_stack_iterator_skip_next(bnfStackIterator); // ref

		ketl_syntax_node* expression = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
		expression->nextSibling = NULL;

		idNode->nextSibling = expression;

		ketl_stack_iterator_skip_next(bnfStackIterator); // ;
		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_IF_ELSE: {
		ketl_stack_iterator_skip_next(bnfStackIterator); // if
		ketl_stack_iterator_skip_next(bnfStackIterator); // (
		ketl_stack_iterator_skip_next(bnfStackIterator); // ref

		ketl_syntax_node* expression = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);

		ketl_stack_iterator_skip_next(bnfStackIterator); // )
		ketl_stack_iterator_skip_next(bnfStackIterator); // ref

		ketl_syntax_node* trueBlock = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
		ketl_syntax_node* falseBlock = NULL;

		ketl_bnf_parser_state* optional = ketl_stack_iterator_get_next(bnfStackIterator);

		ketl_stack_iterator tmpIterator = *bnfStackIterator;
		ketl_bnf_parser_state* next = ketl_stack_iterator_get_next(&tmpIterator); // concat
		if (next->parent == optional) {
			*bnfStackIterator = tmpIterator;
			ketl_stack_iterator_skip_next(bnfStackIterator); // else
			ketl_stack_iterator_skip_next(bnfStackIterator); // ref
			falseBlock = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
			falseBlock->nextSibling = NULL;
		}
		
		ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);

		node->firstChild = expression;
		expression->nextSibling = trueBlock;
		trueBlock->nextSibling = falseBlock;

		node->type = KETL_SYNTAX_NODE_TYPE_IF_ELSE;
		ketl_count_lines(source, state->token->positionInSource + state->tokenOffset, 
			&node->lineInSource, &node->columnInSource);

		node->length = 2;
		if (falseBlock != NULL) {
			node->length = 3;
		}

		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_RETURN: {
		ketl_bnf_parser_state* returnState = ketl_stack_iterator_get_next(bnfStackIterator); // return
		ketl_bnf_parser_state* optional = ketl_stack_iterator_get_next(bnfStackIterator); // optional
				
		KETL_ITERATOR_STACK_PEEK(ketl_bnf_parser_state*, next, *bnfStackIterator); // expression

		ketl_syntax_node* node = ketl_object_pool_get(syntaxNodePool);
		if (next->parent == optional) {
			ketl_stack_iterator_skip_next(bnfStackIterator); // ref
			ketl_syntax_node* expression = ketl_syntax_parse(syntaxNodePool, bnfStackIterator, source);
			expression->nextSibling = NULL;
			node->firstChild = expression;
			node->length = 1;
		}
		else {
			node->firstChild = NULL;
			node->length = 0;
		}

		node->type = KETL_SYNTAX_NODE_TYPE_RETURN;
		ketl_count_lines(source, returnState->token->positionInSource + returnState->tokenOffset, 
			&node->lineInSource, &node->columnInSource);

		ketl_stack_iterator_skip_next(bnfStackIterator); // ;

		return node;
	}
	case KETL_SYNTAX_BUILDER_TYPE_NONE:
	default:
		KETL_DEBUGBREAK();
	}

	return NULL;
}
