//🫖ketl
#include "stack.h"

#include <stdlib.h>

KETL_DEFINE(ketl_stack_bucket_base) {
	ketl_stack_bucket_base* next;
	ketl_stack_bucket_base* prev;
};

static inline ketl_stack_bucket_base* createPoolBase(ketl_stack* stack) {
	ketl_stack_bucket_base* poolBase = malloc(sizeof(ketl_stack_bucket_base) + stack->objectSize * stack->poolSize);
	poolBase->next = NULL;
	return poolBase;
}

void ketl_stack_init(ketl_stack* stack, size_t objectSize, size_t poolSize) {
	// TODO align by adjusting poolSize
	stack->objectSize = objectSize;
	stack->poolSize = poolSize;

	stack->currentPool = createPoolBase(stack);
	stack->currentPool->prev = NULL;
	stack->occupiedObjects = 0;
}

void ketl_stack_deinit(ketl_stack* stack) {
	ketl_stack_bucket_base* it = stack->currentPool;
	while (it->prev) it = it->prev;
	while (it) {
		ketl_stack_bucket_base* next = it->next;
		free(it);
		it = next;
	}
}

bool ketl_stack_is_empty(ketl_stack* stack) {
	return stack->occupiedObjects == 0 && stack->currentPool->prev == NULL;
}

void* ketl_stack_peek(ketl_stack* stack) {
	return ((char*)(stack->currentPool + 1)) + stack->objectSize * (stack->occupiedObjects - 1);
}

void* ketl_stack_push(ketl_stack* stack) {
	if (stack->occupiedObjects >= stack->poolSize) {
		if (stack->currentPool->next) {
			stack->currentPool = stack->currentPool->next;
		}
		else {
			ketl_stack_bucket_base* poolBase = createPoolBase(stack);
			stack->currentPool->next = poolBase;
			poolBase->prev = stack->currentPool;
			stack->currentPool = poolBase;
		}
		stack->occupiedObjects = 0;
	}
	return ((char*)(stack->currentPool + 1)) + stack->objectSize * stack->occupiedObjects++;
}

void ketl_stack_pop(ketl_stack* stack) {
	if (stack->occupiedObjects == 1 && stack->currentPool->prev) {
		stack->currentPool = stack->currentPool->prev;
		stack->occupiedObjects = stack->poolSize;
	}
	else {
		--stack->occupiedObjects;
	}
}

void ketl_stack_reset(ketl_stack* stack) {
	ketl_stack_bucket_base* it = stack->currentPool;
	while (it->prev) it = it->prev;
	stack->currentPool = it;
	stack->occupiedObjects = 0;
}

inline void ketl_stack_iterator_reset(ketl_stack_iterator* iterator) {
	iterator->nextObjectIndex = 0;

	ketl_stack_bucket_base* currentPool = iterator->stack->currentPool;
	while (currentPool->prev) currentPool = currentPool->prev;
	iterator->currentPool = currentPool;
}

void ketl_stack_iterator_init(ketl_stack_iterator* iterator, ketl_stack* stack) {
	iterator->stack = stack;
	ketl_stack_iterator_reset(iterator);
}

bool ketl_stack_iterator_has_next(ketl_stack_iterator* iterator) {
	return iterator->currentPool != NULL && (iterator->currentPool != iterator->stack->currentPool || iterator->nextObjectIndex < iterator->stack->occupiedObjects);
}

void* ketl_stack_iterator_get_next(ketl_stack_iterator* iterator) {
	if (iterator->nextObjectIndex >= iterator->stack->poolSize) {
		iterator->currentPool = iterator->currentPool->next;
		iterator->nextObjectIndex = 0;
	}
	return ((char*)(iterator->currentPool + 1)) + iterator->stack->objectSize * iterator->nextObjectIndex++;
}

void ketl_stack_iterator_skip_next(ketl_stack_iterator* iterator) {
	if (iterator->nextObjectIndex >= iterator->stack->poolSize) {
		iterator->currentPool = iterator->currentPool->next;
		iterator->nextObjectIndex = 1;
	}
	else {
		++iterator->nextObjectIndex;
	}
}