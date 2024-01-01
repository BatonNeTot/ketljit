//🫖ketl
#ifndef ketl_compiler_bnf_parser_h
#define ketl_compiler_bnf_parser_h

#include "ketl/utils.h"

KETL_FORWARD(ketl_stack);
KETL_FORWARD(ketl_token);
KETL_FORWARD(ketl_bnf_node);

KETL_DEFINE(ketl_bnf_parser_state) {
	ketl_bnf_node* bnfNode;
	uint32_t state;
	uint32_t tokenOffset;
	ketl_token* token;
	ketl_bnf_parser_state* parent;
};

KETL_DEFINE(ketl_bnf_error_info) {
	ketl_token* maxToken;
	uint32_t maxTokenOffset;
	ketl_bnf_node* bnfNode;
};

bool ketl_bnf_parse(ketl_stack* bnfStateStack, ketl_bnf_error_info* error);

#endif // ketl_compiler_bnf_parser_h
