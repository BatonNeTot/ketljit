//🫖ketl
#ifndef ketl_compiler_lexer_h
#define ketl_compiler_lexer_h

#include "ketl/utils.h"

KETL_FORWARD(ketl_token);
KETL_FORWARD(ketl_object_pool);

KETL_DEFINE(ketl_lexer) {
	const char* source;
	const char* sourceIt;
	const char* sourceEnd;
	ketl_object_pool* tokenPool;
};

void ketl_lexer_init(ketl_lexer* lexer, const char* source, size_t length, ketl_object_pool* tokenPool);

bool ketl_lexer_has_next_token(const ketl_lexer* lexer);

ketl_token* ketl_lexer_get_next_token(ketl_lexer* lexer);

inline size_t ketl_lexer_current_position(ketl_lexer* lexer) {
	return lexer->sourceIt - lexer->source;
}

#endif // ketl_compiler_lexer_h
