//🫖ketl
#ifndef ketl_compiler_bnf_node_h
#define ketl_compiler_bnf_node_h

#include "ketl/utils.h"

typedef uint8_t ketl_bnf_node_type;

#define KETL_BNF_NODE_TYPE_ID 0
#define KETL_BNF_NODE_TYPE_NUMBER 1
#define KETL_BNF_NODE_TYPE_STRING 2
#define KETL_BNF_NODE_TYPE_CONSTANT 3
#define KETL_BNF_NODE_TYPE_REF 4
#define KETL_BNF_NODE_TYPE_CONCAT 5
#define KETL_BNF_NODE_TYPE_OR 6
#define KETL_BNF_NODE_TYPE_OPTIONAL 7
#define KETL_BNF_NODE_TYPE_REPEAT 8

typedef uint8_t ketl_syntax_builder_type;

#define KETL_SYNTAX_BUILDER_TYPE_NONE 0
#define KETL_SYNTAX_BUILDER_TYPE_BLOCK 1
#define KETL_SYNTAX_BUILDER_TYPE_COMMAND 2
#define KETL_SYNTAX_BUILDER_TYPE_TYPE 3
#define KETL_SYNTAX_BUILDER_TYPE_PRIMARY_EXPRESSION 4
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_1 5
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_2 6
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_3 7
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_4 8
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_5 9
#define KETL_SYNTAX_BUILDER_TYPE_PRECEDENCE_EXPRESSION_6 10
#define KETL_SYNTAX_BUILDER_TYPE_DEFINE_WITH_ASSIGNMENT 11
#define KETL_SYNTAX_BUILDER_TYPE_IF_ELSE 12
#define KETL_SYNTAX_BUILDER_TYPE_RETURN 13

KETL_DEFINE(ketl_bnf_node) {
	uint32_t size;
	ketl_bnf_node_type type;
	ketl_syntax_builder_type builder;
	ketl_bnf_node* nextSibling;
	union {
		const char* value;
		ketl_bnf_node* firstChild;
		ketl_bnf_node* ref;
	};
};

#endif // ketl_compiler_bnf_node_h
