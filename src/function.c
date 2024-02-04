//🫖ketl
#include "function_impl.h"

#include "type_impl.h"
#include "ketl_impl.h"

__asm__(".globl ketl_function_call\n\t"
        ".type ketl_function_call, @function\n\t"
        "ketl_function_call:\n\t"
        ".cfi_startproc\n\t"
        "jmp *(%rdi)\n\t" // rdi == functionClass*
        ".cfi_endproc");

