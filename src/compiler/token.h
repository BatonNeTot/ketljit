//🫖ketl
#ifndef ketl_compiler_token_h
#define ketl_compiler_token_h

#include "ketl/utils.h"

typedef uint8_t ketl_token_type;

#define KETL_TOKEN_TYPE_ID 0
#define KETL_TOKEN_TYPE_STRING 1
#define KETL_TOKEN_TYPE_NUMBER 2
#define KETL_TOKEN_TYPE_SPECIAL 3

KETL_DEFINE(ketl_token) {
	ptrdiff_t positionInSource;
	const char* value;
	uint16_t length;
	ketl_token_type type;
	ketl_token* next;
};

#endif // ketl_compiler_token_h