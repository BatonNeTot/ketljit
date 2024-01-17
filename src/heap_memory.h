//🫖ketl
#ifndef ketl_heap_memory_h
#define ketl_heap_memory_h

#include "managed_memory.h"

#include "containers/int_map.h"
#include "containers/stack.h"

KETL_DEFINE(ketl_heap_memory) {
    ketl_int_map variableMap;
    ketl_stack staticRootVariables;
    ketl_stack markStack;
    bool currentUsageState;
};

void ketl_heap_memory_init(ketl_heap_memory* hmemory);

void ketl_heap_memory_deinit(ketl_heap_memory* hmemory);

void* ketl_heap_memory_allocate(ketl_heap_memory* hmemory, ketl_type_pointer type, ketl_variable_traits traits);

void* ketl_heap_memory_allocate_root(ketl_heap_memory* hmemory, ketl_type_pointer type, ketl_variable_traits traits);

void ketl_heap_memory_prune(ketl_heap_memory* hmemory);

#endif // ketl_heap_memory_h