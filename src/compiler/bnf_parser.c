//🫖ketl
#include "bnf_parser.h"

#include "token.h"
#include "bnf_node.h"
#include "containers/object_pool.h"
#include "containers/stack.h"

static bool iterateIterator(ketl_token** pToken, uint32_t* pTokenOffset, const char* value, uint32_t valueLength) {
	ketl_token* token = *pToken;
	uint32_t tokenOffset = *pTokenOffset;

	const char* tokenValue = token->value;
	uint32_t tokenLength = token->length;

	uint32_t tokenDiff = tokenLength - tokenOffset;

	if (valueLength > tokenDiff) {
		return false;
	}

	for (uint32_t i = 0u; i < valueLength; ++i) {
		if (value[i] != tokenValue[i + tokenOffset]) {
			return false;
		}
	}

	if (valueLength == tokenDiff) {
		*pToken = token->next;
		*pTokenOffset = 0;
	}
	else {
		*pTokenOffset += valueLength;
	}

	return true;
}

static inline bool iterate(ketl_bnf_parser_state* solverState, ketl_token** pToken, uint32_t* pTokenOffset) {
	switch (solverState->bnfNode->type) {
	case KETL_BNF_NODE_TYPE_ID: {
		ketl_token* currentToken = *pToken;
		if (currentToken != NULL && currentToken->type == KETL_TOKEN_TYPE_ID) {
			*pToken = currentToken->next;
			return true;
		}
		else {
			return false;
		}
	}
	case KETL_BNF_NODE_TYPE_NUMBER: {
		ketl_token* currentToken = *pToken;
		if (currentToken != NULL && currentToken->type == KETL_TOKEN_TYPE_NUMBER) {
			*pToken = currentToken->next;
			return true;
		}
		else {
			return false;
		}
	}
	case KETL_BNF_NODE_TYPE_STRING: {
		ketl_token* currentToken = *pToken;
		if (currentToken != NULL && currentToken->type == KETL_TOKEN_TYPE_STRING) {
			*pToken = currentToken->next;
			return true;
		}
		else {
			return false;
		}
	}
	case KETL_BNF_NODE_TYPE_CONSTANT:{
		ketl_token* currentToken = *pToken;
		if (currentToken == NULL) {
			return false;
		}

		switch (currentToken->type) {
		case KETL_TOKEN_TYPE_SPECIAL: {
			uint32_t nodeLength = solverState->bnfNode->size;

			if (!iterateIterator(pToken, pTokenOffset, solverState->bnfNode->value, nodeLength)) {
				return false;
			}
			return true;
		}
		case KETL_TOKEN_TYPE_ID: {
			uint32_t tokenLength = currentToken->length;
			uint32_t nodeLength = solverState->bnfNode->size;

			if (tokenLength != nodeLength) {
				return false;
			}

			if (!iterateIterator(pToken, pTokenOffset, solverState->bnfNode->value, nodeLength)) {
				return false;
			}
			return true;
		}
		default: {
			return false;
		}
		}
	}
	default: {
		solverState->state = 0;
		return true;
	}
	}
}

static inline ketl_bnf_node* nextChild(ketl_bnf_parser_state* solverState) {
	switch (solverState->bnfNode->type) {
	case KETL_BNF_NODE_TYPE_CONSTANT:{
		return NULL;
	}
	case KETL_BNF_NODE_TYPE_REF: {
		if (solverState->state > 0) {
			return NULL;
		}
		++solverState->state;
		return solverState->bnfNode->ref;
	}
	case KETL_BNF_NODE_TYPE_CONCAT: {
		uint32_t state = solverState->state;
		if (state < solverState->bnfNode->size) {
			++solverState->state;
			ketl_bnf_node* it = solverState->bnfNode->firstChild;
			for (uint32_t i = 0; i < state; ++i) {
				it = it->nextSibling;
			}
			return it;
		}
		else {
			return NULL;
		}
	}
	case KETL_BNF_NODE_TYPE_OR:
	case KETL_BNF_NODE_TYPE_OPTIONAL: {
		uint32_t size = solverState->bnfNode->size;
		uint32_t state = solverState->state;
		if (state >= size) {
			return NULL;
		}
		solverState->state += size;
		ketl_bnf_node* it = solverState->bnfNode->firstChild;
		for (uint32_t i = 0; i < state; ++i) {
			it = it->nextSibling;
		}
		return it;
	}
	case KETL_BNF_NODE_TYPE_REPEAT: {
		if (solverState->state == 1) {
			solverState->state = 0;
			return NULL;
		}
		return solverState->bnfNode->firstChild;
	}
	default:
		return NULL;
	}
}

static inline bool childRejected(ketl_bnf_parser_state* solverState) {
	switch (solverState->bnfNode->type) {
	case KETL_BNF_NODE_TYPE_CONCAT: {
		--solverState->state;
		return false;
	}
	case KETL_BNF_NODE_TYPE_OR: {
		uint32_t size = solverState->bnfNode->size;
		return (solverState->state -= size - 1) < size;
	}
	case KETL_BNF_NODE_TYPE_OPTIONAL: {
		uint32_t size = solverState->bnfNode->size;
		return (solverState->state -= size - 1) <= size;
	}
	case KETL_BNF_NODE_TYPE_REPEAT: {
		solverState->state = 1;
		return true;
	}
	default:
		return false;
	}
}

#if KETL_OS_WINDOWS
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define PRINT_SPACE(x) printf("%*s", (x), " ")

static void printBnfSolution(ketl_stack_iterator* iterator) {
	system("cls");

	HANDLE  hConsole;
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	ketl_stack parentStack;
	ketl_stack_init(&parentStack, sizeof(void*), 16);
	int deltaOffset = 4;
	int currentOffset = 0;
	while (ketl_stack_iterator_has_next(iterator)) {
		ketl_bnf_parser_state* solverState = ketl_stack_iterator_get_next(iterator);

		ketl_bnf_parser_state* peeked;
		while (!ketl_stack_is_empty(&parentStack) && solverState->parent != (peeked = *(ketl_bnf_parser_state**)ketl_stack_peek(&parentStack))) {
			ketl_stack_pop(&parentStack);
			switch (peeked->bnfNode->type) {
			case KETL_BNF_NODE_TYPE_REF:
			case KETL_BNF_NODE_TYPE_OR:
			case KETL_BNF_NODE_TYPE_OPTIONAL:
				//break;
			default:
				currentOffset -= deltaOffset;
			}
		}

		if (solverState->token == NULL) {
			break;
		}
		switch (solverState->bnfNode->type) {
		case KETL_BNF_NODE_TYPE_REF:
			(*(ketl_bnf_parser_state**)ketl_stack_push(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("REF ");
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
			printf("%.*s ", solverState->token->length - solverState->tokenOffset, solverState->token->value + solverState->tokenOffset);
			SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE);
			printf("%d\n", solverState->bnfNode->ref->builder);
			SetConsoleTextAttribute(hConsole, 15);
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_CONCAT:
			(*(ketl_bnf_parser_state**)ketl_stack_push(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("CONCAT ");
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
			printf("%.*s\n", solverState->token->length - solverState->tokenOffset, solverState->token->value + solverState->tokenOffset);
			SetConsoleTextAttribute(hConsole, 15);
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_OR:
			(*(ketl_bnf_parser_state**)ketl_stack_push(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("OR ");
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
			printf("%.*s\n", solverState->token->length - solverState->tokenOffset, solverState->token->value + solverState->tokenOffset);
			SetConsoleTextAttribute(hConsole, 15);
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_OPTIONAL:
			(*(ketl_bnf_parser_state**)ketl_stack_push(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("OPTIONAL ");
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
			printf("%.*s\n", solverState->token->length - solverState->tokenOffset, solverState->token->value + solverState->tokenOffset);
			SetConsoleTextAttribute(hConsole, 15);
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_REPEAT:
			(*(ketl_bnf_parser_state**)ketl_stack_push(&parentStack)) = solverState;
			PRINT_SPACE(currentOffset);
			printf("REPEAT ");
			SetConsoleTextAttribute(hConsole, FOREGROUND_RED);
			printf("%.*s\n", solverState->token->length - solverState->tokenOffset, solverState->token->value + solverState->tokenOffset);
			SetConsoleTextAttribute(hConsole, 15);
			currentOffset += deltaOffset;
			break;
		case KETL_BNF_NODE_TYPE_CONSTANT:
			//break;
		default:
			PRINT_SPACE(currentOffset);
			SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN);
			printf("%.*s\n", solverState->token->length - solverState->tokenOffset, solverState->token->value + solverState->tokenOffset);
			SetConsoleTextAttribute(hConsole, 15);
		}
	}
	ketl_stack_deinit(&parentStack);
	ketl_stack_iterator_reset(iterator);
}

static inline void drawStack(ketl_stack* bnfStateStack) {
	ketl_stack_iterator iterator;
	ketl_stack_iterator_init(&iterator, bnfStateStack);
	printBnfSolution(&iterator);
}

#else

static inline void drawStack(ketl_stack* bnfStateStack) {
	(void)bnfStateStack;
}

#endif

bool ketl_bnf_parse(ketl_stack* bnfStateStack, ketl_bnf_error_info* error) {
	drawStack(bnfStateStack);
	while (!ketl_stack_is_empty(bnfStateStack)) {
		ketl_bnf_parser_state* current = ketl_stack_peek(bnfStateStack);

		ketl_token* currentToken = current->token;
		uint32_t currentTokenOffset = current->tokenOffset;

		if (!iterate(current, &currentToken, &currentTokenOffset)) {
			if (error->maxToken == currentToken && error->maxTokenOffset == currentTokenOffset) {
				error->bnfNode = current->bnfNode;
			}
			else if (error->maxToken != NULL && (currentToken == NULL ||
				error->maxToken->positionInSource + error->maxTokenOffset
				< currentToken->positionInSource + currentTokenOffset)) {
				error->maxToken = currentToken;
				error->maxTokenOffset = currentTokenOffset;
				error->bnfNode = current->bnfNode;
			}
			KETL_FOREVER {
				ketl_stack_pop(bnfStateStack);
				drawStack(bnfStateStack);
				ketl_bnf_parser_state* parent = current->parent;
				if (parent && childRejected(parent)) {
					current = parent;
					break;
				}
				if (ketl_stack_is_empty(bnfStateStack)) {
					return (currentToken == NULL);
				}
				current = ketl_stack_peek(bnfStateStack);
				currentToken = current->token;
			}
		}

		ketl_bnf_node* next;
		KETL_FOREVER {
			next = nextChild(current);

			if (next != NULL) {
				ketl_bnf_parser_state* pushed = ketl_stack_push(bnfStateStack);

				pushed->bnfNode = next;
				pushed->token = currentToken;
				pushed->tokenOffset = currentTokenOffset;

				pushed->parent = current;
				drawStack(bnfStateStack);
				break;
			}

			current = current->parent;
			if (current == NULL) {
				return (currentToken == NULL);
			}
		}
	}

	return false;
}
