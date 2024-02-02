//🫖ketl
#ifndef ketl_compiler_ir_compiler_h
#define ketl_compiler_ir_compiler_h

#include "../type_impl.h"

#include "ketl/utils.h"

#include "executable_memory.h"
#include "containers/int_map.h"
#include "containers/stack.h"

KETL_FORWARD(ketl_function);
KETL_FORWARD(ketl_ir_function);

KETL_DEFINE(ketl_ir_compiler) {
	ketl_executable_memory exeMemory;
    ketl_int_map operationBufferOffsetMap;
    ketl_stack jumpList;
};

void ketl_ir_compiler_init(ketl_ir_compiler* irCompiler);

void ketl_ir_compiler_deinit(ketl_ir_compiler* irCompiler);

const uint8_t* ketl_ir_compiler_compile(ketl_ir_compiler* irCompiler, ketl_ir_function* irFunction, ketl_type_parameters* parameters, uint16_t parameterCount);

#endif // ketl_compiler_ir_compiler_h
