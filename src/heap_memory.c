//🫖ketl
#include "heap_memory.h"

#include "type_impl.h"

#include <stdlib.h>

KETL_DEFINE(ketl_heap_mamory_variable) {
    void* ptr;
    ketl_type_pointer type;
    ketl_variable_traits traits;
    bool managedFlag;
    bool usageState;
};

static void destroy_variable(ketl_heap_mamory_variable* variable) {
    // TODO call destructor
    free(variable->ptr);
}

static inline ketl_heap_mamory_variable* find_node(ketl_heap_memory* hmemory, void* ptr) {
    return ketl_int_map_get(&hmemory->variableMap, (ketl_int_map_key)ptr);
} 

static void memory_mark(ketl_heap_memory* hmemory) {
    {
        ketl_stack_iterator rootIterator;
        ketl_stack_iterator_init(&rootIterator, &hmemory->staticRootVariables);
        while (ketl_stack_iterator_has_next(&rootIterator)) {
            (*(ketl_heap_mamory_variable**)ketl_stack_push(&hmemory->markStack)) =
                 *(ketl_heap_mamory_variable**)ketl_stack_iterator_get_next(&rootIterator);
        }
    }

    bool currentUsageState = hmemory->currentUsageState;
    while(!ketl_stack_is_empty(&hmemory->markStack)) {
        ketl_heap_mamory_variable* variableToMark = *(ketl_heap_mamory_variable**)ketl_stack_peek(&hmemory->markStack);
        ketl_stack_pop(&hmemory->markStack);

        if (variableToMark->usageState == currentUsageState) {
            continue;
        }

        variableToMark->usageState = currentUsageState;

        ketl_heap_mamory_variable* typeVariable = find_node(hmemory, variableToMark->type.base);
        if (typeVariable != NULL) {
            (*(ketl_heap_mamory_variable**)ketl_stack_push(&hmemory->markStack)) = typeVariable;
        }

        // TODO go through fields and collect for marking
    }
}

static void memory_sweep(ketl_heap_memory* hmemory) {
    bool currentUsagestate = hmemory->currentUsageState;

    ketl_int_map_iterator variableIterator;
    ketl_int_map_iterator_init(&variableIterator, &hmemory->variableMap);
    while (ketl_int_map_iterator_has_next(&variableIterator)) {
        void* ptr;
        ketl_heap_mamory_variable** ppVariable;
        ketl_int_map_iterator_get(&variableIterator, (ketl_int_map_key*)(&ptr), &ppVariable);

        ketl_heap_mamory_variable variable = **ppVariable;

        if (ptr == variable.ptr && variable.usageState != currentUsagestate) {
            destroy_variable(&variable);
            ketl_int_map_iterator_remove(&variableIterator);
        } else {
            ketl_int_map_iterator_next(&variableIterator);
        }
    }
}

void ketl_heap_memory_init(ketl_heap_memory* hmemory) {
    ketl_int_map_init(&hmemory->variableMap, sizeof(ketl_heap_mamory_variable),32);
    ketl_stack_init(&hmemory->staticRootVariables, sizeof(ketl_heap_mamory_variable*),32);
    ketl_stack_init(&hmemory->markStack, sizeof(ketl_heap_mamory_variable*),32);
}

void ketl_heap_memory_deinit(ketl_heap_memory* hmemory) {
    while (!ketl_stack_is_empty(&hmemory->staticRootVariables)) {
        ketl_heap_mamory_variable* variable = *(ketl_heap_mamory_variable**)ketl_stack_peek(&hmemory->staticRootVariables);
        ketl_stack_pop(&hmemory->staticRootVariables);
        
        destroy_variable(variable);
    }

    hmemory->currentUsageState ^= -1;
    memory_sweep(hmemory);

    ketl_stack_deinit(&hmemory->markStack);
    ketl_stack_deinit(&hmemory->staticRootVariables);
    ketl_int_map_deinit(&hmemory->variableMap);
}

static ketl_heap_mamory_variable* allocate_node(ketl_heap_memory* hmemory, ketl_type_pointer type, ketl_variable_traits traits) {
    ketl_heap_mamory_variable variable;
    variable.ptr = malloc(ketl_type_get_size(type));
    variable.type = type;
    variable.traits = traits;
    variable.managedFlag = true;
    variable.usageState = hmemory->currentUsageState;

    ketl_heap_mamory_variable* pVariable;
    ketl_int_map_get_or_create(&hmemory->variableMap, (ketl_int_map_key)variable.ptr, &pVariable);
    *pVariable = variable;

    return pVariable;
}

void* ketl_heap_memory_allocate(ketl_heap_memory* hmemory, ketl_type_pointer type, ketl_variable_traits traits) {
    ketl_heap_mamory_variable* variable = allocate_node(hmemory, type, traits);
    return variable->ptr;
}

void* ketl_heap_memory_allocate_root(ketl_heap_memory* hmemory, ketl_type_pointer type, ketl_variable_traits traits) {
    ketl_heap_mamory_variable* variable = allocate_node(hmemory, type, traits);
    (*(ketl_heap_mamory_variable**)ketl_stack_push(&hmemory->staticRootVariables)) = variable;
    return variable->ptr;
}

void ketl_heap_memory_prune(ketl_heap_memory* hmemory) {
    hmemory->currentUsageState ^= -1;

    memory_mark(hmemory);
    memory_sweep(hmemory);
}