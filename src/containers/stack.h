//🫖ketl
#ifndef ketl_stack_h
#define ketl_stack_h

#include "ketl/utils.h"

KETL_FORWARD(ketl_stack_bucket_base);

KETL_DEFINE(ketl_stack) {
	size_t objectSize;
	size_t poolSize;
	ketl_stack_bucket_base* currentPool;
	size_t occupiedObjects;
};

void ketl_stack_init(ketl_stack* stack, size_t objectSize, size_t poolSize);

void ketl_stack_deinit(ketl_stack* stack);

bool ketl_stack_is_empty(ketl_stack* stack);

void* ketl_stack_push(ketl_stack* stack);

void* ketl_stack_peek(ketl_stack* stack);

void ketl_stack_pop(ketl_stack* stack);

void ketl_stack_reset(ketl_stack* stack);

KETL_DEFINE(ketl_stack_iterator) {
	ketl_stack* stack;
	ketl_stack_bucket_base* currentPool;
	size_t nextObjectIndex;
};

void ketl_stack_iterator_init(ketl_stack_iterator* iterator, ketl_stack* stack);

void ketl_stack_iterator_reset(ketl_stack_iterator* iterator);

bool ketl_stack_iterator_has_next(ketl_stack_iterator* iterator);

void* ketl_stack_iterator_get_next(ketl_stack_iterator* iterator);

void ketl_stack_iterator_skip_next(ketl_stack_iterator* iterator);

// TODO remove it; afterthought, why?
#define KETL_ITERATOR_STACK_PEEK(variableName, iterator)\
do { \
ketl_stack_iterator __temp = (iterator);\
variableName = ketl_stack_iterator_get_next(&__temp);\
} while (0)

#define KETL_ITERATOR_STACK_PEEK_VARIABLE(variableType, variableName, iterator)\
variableType variableName = NULL;\
KETL_ITERATOR_STACK_PEEK(variableName, iterator)

#endif // ketl_stack_h
