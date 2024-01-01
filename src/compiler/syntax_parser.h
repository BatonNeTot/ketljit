//🫖ketl
#ifndef ketl_compiler_syntax_parser_h
#define ketl_compiler_syntax_parser_h

#include "ketl/utils.h"

KETL_FORWARD(ketl_object_pool);
KETL_FORWARD(ketl_stack_iterator);
KETL_FORWARD(ketl_syntax_node);

void ketl_count_lines(const char* source, uint64_t length, uint32_t* pLine, uint32_t* pColumn);

ketl_syntax_node* ketl_syntax_parse(ketl_object_pool* syntaxNodePool, ketl_stack_iterator* bnfStackIterator, const char* source);

#endif // ketl_compiler_syntax_parser_h
