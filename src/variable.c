//🫖ketl
#include "variable_impl.h"

#include "function_impl.h"


int64_t ketl_variable_get_as_i64(ketl_variable* variable) {
    switch (variable->irVariable.value.type) {
    case KETL_IR_ARGUMENT_TYPE_REFERENCE:
        return *(int64_t*)variable->irVariable.value.pointer;
    case KETL_IR_ARGUMENT_TYPE_INT8:
        return variable->irVariable.value.int8;
    case KETL_IR_ARGUMENT_TYPE_INT16:
        return variable->irVariable.value.int16;
    case KETL_IR_ARGUMENT_TYPE_INT32:
        return variable->irVariable.value.int32;
    case KETL_IR_ARGUMENT_TYPE_INT64:
        return variable->irVariable.value.int64;
    }
    return 0;
}

#include <stdlib.h>

void ketl_variable_free(ketl_variable* variable) {
    // TODO deal with gc
    free(variable);
}

__asm__(".globl ketl_variable_call_void\n\t"
        ".type ketl_variable_call_void, @function\n\t"
        "ketl_variable_call_void:\n\t"
        ".globl ketl_variable_call_u64\n\t"
        ".type ketl_variable_call_u64, @function\n\t"
        "ketl_variable_call_u64:\n\t"
        ".globl ketl_variable_call_i64\n\t"
        ".type ketl_variable_call_i64, @function\n\t"
        "ketl_variable_call_i64:\n\t"
        ".cfi_startproc\n\t"
        "movq (%rdi), %rdi\n\t" // rdi == functionClass*
        "jmp ketl_function_call\n\t"
        ".cfi_endproc");