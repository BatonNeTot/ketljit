//🫖ketl
#include "function_impl.h"

#include "type_impl.h"
#include "ketl_impl.h"

__asm__(".globl ketl_function_call\n\t"
#if !KETL_OS_WINDOWS
        ".type ketl_function_call, @function\n\t"
#endif
        "ketl_function_call:\n\t"
        ".cfi_startproc\n\t"
#if !KETL_OS_WINDOWS
        "jmp *(%rdi)\n\t" // rdi == functionClass*
#else
        "jmp *(%rcx)\n\t" // rcx == functionClass*
#endif
        ".cfi_endproc");

