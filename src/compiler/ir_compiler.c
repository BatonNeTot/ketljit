//🫖ketl
#include "ir_compiler.h"

#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
#include "ketl/function.h"
#include "ketl/type.h"

#include <string.h>

#include <stdio.h>

#define KETL_SIZE_8B    0
#define KETL_SIZE_16B   1
#define KETL_SIZE_32B   2
#define KETL_SIZE_64B   3


#define KETL_REG_AL     0
#define KETL_REG_CL     1
#define KETL_REG_DL     2
#define KETL_REG_BL     3
#define KETL_REG_AH     4
#define KETL_REG_CH     5
#define KETL_REG_DH     6
#define KETL_REG_BH     7

#define KETL_REG_AX     0
#define KETL_REG_CX     1
#define KETL_REG_DX     2
#define KETL_REG_BX     3
#define KETL_REG_SP     4
#define KETL_REG_BP     5
#define KETL_REG_SI     6
#define KETL_REG_DI     7
#define KETL_REG_R8     8
#define KETL_REG_R9     9
#define KETL_REG_R10    10
#define KETL_REG_R11    11
#define KETL_REG_R12    12
#define KETL_REG_R13    13
#define KETL_REG_R14    14
#define KETL_REG_R15    15

#define KETL_ARG_REG            0
#define KETL_ARG_REG_MEM        1
#define KETL_ARG_RSP_MEM_DISP   2
#define KETL_ARG_IMM            3
#define KETL_ARG_IMM_MEM        4

#define KETL_OP_MOV     0

typedef struct OpStruct {
    uint8_t size;
    uint8_t firstArgType;
    uint8_t secondArgType;
    uint16_t opCode;
    uint64_t firstArg;
    uint64_t secondArg;
} OpStruct;

static uint64_t execMov(uint8_t* buffer, OpStruct* opStruct) {
    uint64_t size = 0;
    switch(opStruct->size) {
    case KETL_SIZE_16B: 
        buffer[size++] = 0x66;
        break;
    case KETL_SIZE_64B: 
        buffer[size++] = 0x48;
        break;
    }
    switch (opStruct->firstArgType) {
    case KETL_ARG_REG:
        switch (opStruct->secondArgType) {
        case KETL_ARG_REG:
            if (opStruct->size == KETL_SIZE_8B) {
                buffer[size++] = 0x88;
            } else {
                buffer[size++] = 0x89;
            }
            buffer[size++] = 0xc0 + opStruct->firstArg + (opStruct->secondArg << 3);
            return size;
        case KETL_ARG_IMM:
            if (opStruct->size == KETL_SIZE_8B) {
                buffer[size++] = 0xb0 + opStruct->firstArg;
            } else {
                buffer[size++] = 0xb8 + opStruct->firstArg;
            }
            switch (opStruct->size) {
            case KETL_SIZE_8B: {
                *(uint8_t*)(buffer + size) = opStruct->secondArg;
                return size + 1;
            }
            case KETL_SIZE_16B: {
                *(uint16_t*)(buffer + size) = opStruct->secondArg;
                return size + 2;
            }
            case KETL_SIZE_32B: {
                *(uint32_t*)(buffer + size) = opStruct->secondArg;
                return size + 4;
            }
            case KETL_SIZE_64B: {
                *(uint64_t*)(buffer + size) = opStruct->secondArg;
                return size + 8;
            }
            KETL_NODEFAULT()
            }
        case KETL_ARG_RSP_MEM_DISP:
            if (opStruct->size == KETL_SIZE_8B) {
                buffer[size++] = 0x8a;
            } else {
                buffer[size++] = 0x8b;
            }
            buffer[size++] = 0x84 + (opStruct->firstArg << 3);
            buffer[size++] = 0x24;
            *(uint32_t*)(buffer + size) = opStruct->secondArg;
            return size + 4;
        case KETL_ARG_IMM_MEM:
            if (opStruct->size == KETL_SIZE_8B) {
                buffer[size++] = 0xa0;
            } else {
                buffer[size++] = 0xa1;
            }
            *(uint64_t*)(buffer + size) = opStruct->secondArg;
            size += 8;
            if (opStruct->firstArg == KETL_REG_AX) {
                return size;
            } else {
                opStruct->secondArgType = KETL_ARG_REG;
                opStruct->secondArg = KETL_REG_AX;
                return size + execMov(buffer + size, opStruct);
            }
        KETL_NODEFAULT()
        }
    }
    return 0;
}

static uint64_t execOpcode(uint8_t* buffer, OpStruct* opStruct) {
    switch (opStruct->opCode) {
    case KETL_OP_MOV:
        return execMov(buffer, opStruct);
    }
    return 0;
}

#define EXEC_OPCODE_REG_IMM(buffer, sizeAccum, sizeArg, opcodeArg, regArg, immArg) do {\
    OpStruct tmpOpStruct;\
    tmpOpStruct.size = (sizeArg);\
    tmpOpStruct.firstArgType = KETL_ARG_REG;\
    tmpOpStruct.secondArgType = KETL_ARG_IMM;\
    tmpOpStruct.opCode = (opcodeArg);\
    tmpOpStruct.firstArg = (regArg);\
    tmpOpStruct.secondArg = (immArg);\
    (sizeAccum) += execOpcode((buffer), &tmpOpStruct);\
} while(false)

#define EXEC_OPCODE_REG_RSP_DISP(buffer, sizeAccum, sizeArg, opcodeArg, regArg, immArg) do {\
    OpStruct tmpOpStruct;\
    tmpOpStruct.size = (sizeArg);\
    tmpOpStruct.firstArgType = KETL_ARG_REG;\
    tmpOpStruct.secondArgType = KETL_ARG_RSP_MEM_DISP;\
    tmpOpStruct.opCode = (opcodeArg);\
    tmpOpStruct.firstArg = (regArg);\
    tmpOpStruct.secondArg = (immArg);\
    (sizeAccum) += execOpcode((buffer), &tmpOpStruct);\
} while(false)

#define EXEC_OPCODE_REG_IMM_MEM(buffer, sizeAccum, sizeArg, opcodeArg, regArg, immArg) do {\
    OpStruct tmpOpStruct;\
    tmpOpStruct.size = (sizeArg);\
    tmpOpStruct.firstArgType = KETL_ARG_REG;\
    tmpOpStruct.secondArgType = KETL_ARG_IMM_MEM;\
    tmpOpStruct.opCode = (opcodeArg);\
    tmpOpStruct.firstArg = (regArg);\
    tmpOpStruct.secondArg = (immArg);\
    (sizeAccum) += execOpcode((buffer), &tmpOpStruct);\
} while(false)

static inline uint64_t loadArgumentIntoReg(uint8_t* buffer, ketl_ir_argument* argument, uint64_t stackUsage, uint8_t sizeMacro, uint8_t regMacro) {
    uint64_t size = 0;
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: 
        EXEC_OPCODE_REG_RSP_DISP(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, (int32_t)argument->stack);
        return size;
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES:
        EXEC_OPCODE_REG_RSP_DISP(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, (int32_t)(stackUsage + sizeof(void*) + argument->stack));
        return size;
    case KETL_IR_ARGUMENT_TYPE_POINTER: 
        EXEC_OPCODE_REG_IMM_MEM(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, (uint64_t)argument->pointer);
        return size;
    case KETL_IR_ARGUMENT_TYPE_INT8:
        EXEC_OPCODE_REG_IMM(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int8);
        return size;
    case KETL_IR_ARGUMENT_TYPE_INT16:
        EXEC_OPCODE_REG_IMM(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int16);
        return size;
    case KETL_IR_ARGUMENT_TYPE_INT32:
        EXEC_OPCODE_REG_IMM(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int32);
        return size;
    case KETL_IR_ARGUMENT_TYPE_INT64:
        EXEC_OPCODE_REG_IMM(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, argument->int32);
        return size;
    KETL_NODEFAULT()
    }
    KETL_DEBUGBREAK();
    return 0;
}

static uint64_t loadEaxIntoArgument(uint8_t* buffer, ketl_ir_argument* argument) {
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: {
        const uint8_t opcodesArray[] =
        {
             0x89, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     DWORD PTR [rsp + 65535], eax
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 3) = (int32_t)argument->stack;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_POINTER: {
        const uint8_t opcodesArray[] =
        {
             0xa3, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,         // mov    DWORD PTR [65535], eax
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(uint64_t*)(buffer + 2) = (uint64_t)argument->pointer;
        return sizeof(opcodesArray);
    }
    }
    KETL_DEBUGBREAK();
    return 0;
}

static uint64_t loadRaxIntoArgument(uint8_t* buffer, ketl_ir_argument* argument) {
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0x89, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     QWORD PTR [rsp + 65535], rax
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 4) = (int32_t)argument->stack;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_POINTER: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0xa3, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,         // mov    QWORD PTR [65535], rax
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(uint64_t*)(buffer + 2) = (uint64_t)argument->pointer;
        return sizeof(opcodesArray);
    }
    }
    KETL_DEBUGBREAK();
    return 0;
}

static uint64_t compareRaxAndRcx(uint8_t* buffer) {
    const uint8_t opcodesArray[] =
    {
        0x48, 0x39, 0xc8,               // cmp     rax, rcx                         
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
    return sizeof(opcodesArray);
}


static uint64_t copyArgumentsToStack(uint8_t* buffer, ketl_ir_function* irFunction) {
    uint64_t length = 0;
    ketl_ir_operation* itOperation = irFunction->operations;
    ketl_ir_argument* functionArgument = irFunction->arguments;
    if (!functionArgument) {
        return 0;
    }

    if (functionArgument == (void*)itOperation) {
        return length;
    }
    switch (functionArgument->type) {
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_1_BYTE: {
        const uint8_t opcodesArray[] =
        {
            0x88, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     BYTE PTR [rsp + 65535], cl
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 3) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_2_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x66, 0x89, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     WORD PTR [rsp + 65535], cx
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x89, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     DWORD PTR [rsp + 65535], ecx
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 3) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_8_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x48, 0x89, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     QWORD PTR [rsp + 65535], rcx
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    default: 
        return length;
    }

    if (functionArgument == (void*)itOperation) {
        return length;
    }
    switch (functionArgument->type) {
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_1_BYTE: {
        const uint8_t opcodesArray[] =
        {
            0x88, 0x94, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     BYTE PTR [rsp + 65535], dl
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 3) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_2_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x66, 0x89, 0x94, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     WORD PTR [rsp + 65535], dx
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x89, 0x94, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     DWORD PTR [rsp + 65535], edx
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 3) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_8_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x48, 0x89, 0x94, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     QWORD PTR [rsp + 65535], rdx
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    default:
        return length;
    }

    if (functionArgument == (void*)itOperation) {
        return length;
    }
    switch (functionArgument->type) {
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_1_BYTE: {
        const uint8_t opcodesArray[] =
        {
            0x44, 0x88, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     BYTE PTR [rsp + 65535], r8b
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_2_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x66, 0x44, 0x89, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     WORD PTR [rsp + 65535], r8w
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 5) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x44, 0x89, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     DWORD PTR [rsp + 65535], r8d
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_8_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x4c, 0x89, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     QWORD PTR [rsp + 65535], r8
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    default:
        return length;
    }

    if (functionArgument == (void*)itOperation) {
        return length;
    }
    switch (functionArgument->type) {
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_1_BYTE: {
        const uint8_t opcodesArray[] =
        {
            0x44, 0x88, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     BYTE PTR [rsp + 65535], r9b
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_2_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x66, 0x44, 0x89, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     WORD PTR [rsp + 65535], r9w
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 5) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES: {
        const uint8_t opcodesArray[] =
        {
           0x44, 0x89, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     DWORD PTR [rsp + 65535], r9d
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 3) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_8_BYTES: {
        const uint8_t opcodesArray[] =
        {
            0x4c, 0x89, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,           // mov     QWORD PTR [rsp + 65535], r9
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 4) = (int32_t)(sizeof(void*) + functionArgument->stack);
        length += sizeof(opcodesArray);
        ++functionArgument;
        break;
    }
    default:
        return length;
    }

    return length;
}


KETL_DEFINE(JumpInfo) {
    uint64_t bufferOffset;
    uint64_t fromOffset;
    ketl_ir_operation* to;
};


ketl_function* ketl_ir_compiler_compile(ketl_ir_compiler* irCompiler, ketl_ir_function* irFunction) {
    ketl_int_map_reset(&irCompiler->operationBufferOffsetMap);
    ketl_stack_reset(&irCompiler->jumpList);

    uint64_t stackUsage = irFunction->stackUsage;
    stackUsage = ((stackUsage + 15) / 16) * 16; // 16 bites aligned

    uint8_t opcodesBuffer[4096];
    uint64_t length = 0;


    length = copyArgumentsToStack(opcodesBuffer, irFunction);
    if (stackUsage > 0) {
        const uint8_t opcodesArray[] =
        {
            0x48, 0x81, 0xec, 0xff, 0xff, 0x00, 0x00,             // sub     rsp, 65535
        };
        memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(opcodesBuffer + length + 3) = (int32_t)stackUsage;
        length += sizeof(opcodesArray);
    }

    ketl_ir_operation* itOperation = irFunction->operations;
    for (uint64_t i = 0; i < irFunction->operationsCount; ++i) {
        {
            uint64_t* offset;
            ketl_int_map_get_or_create(&irCompiler->operationBufferOffsetMap, (ketl_int_map_key)&itOperation[i], &offset);
            *offset = length;
        }

        switch (itOperation[i].code) {
        case KETL_IR_CODE_CAST_INT32_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_32B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x98,   // cdqe // signed extending eax to rax                                
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_ADD_INT32: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage, KETL_SIZE_32B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_32B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x01, 0xc8,   // add eax, ecx                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadEaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_ADD_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage, KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x01, 0xc8,   // add rax, rcx                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_SUB_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage, KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x29, 0xc8,    // sub rax, rcx                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_MULTY_INT32: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage, KETL_SIZE_32B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_32B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0xf7, 0xe9,   // imul ecx // edx:eax = ecx * eax                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadEaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_MULTY_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage, KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0xf7, 0xe9,   // imul rcx // rdx:rax = rcx * rax                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_EQUAL_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage, KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            length += compareRaxAndRcx(opcodesBuffer + length);
            {
                const uint8_t opcodesArray[] =
                {
                    0x0f, 0x94, 0xc0,                                                               // sete al
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_UNEQUAL_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage, KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            length += compareRaxAndRcx(opcodesBuffer + length);
            {
                const uint8_t opcodesArray[] =
                {
                    0x0f, 0x95, 0xc0,                                                               // setne al
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_ASSIGN_8_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[1]);
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_COPY_4_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_32B, KETL_REG_AX);
            length += loadEaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_COPY_8_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_JUMP_IF_FALSE: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[0], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x85, 0xc0,                                   // test rax, rax
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            {
                const uint8_t opcodesArray[] =
                {
                    0x0f, 0x84, 0xf9, 0xff, 0x00, 0x00,                 // je .65535
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));

                JumpInfo jumpInfo;
                jumpInfo.bufferOffset = length + 2;
                length += sizeof(opcodesArray);
                jumpInfo.fromOffset = length;
                jumpInfo.to = itOperation[i].extraNext;
                *(JumpInfo*)ketl_stack_push(&irCompiler->jumpList) = jumpInfo;
            }
            break;
        }
        case KETL_IR_CODE_RETURN_8_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[0], stackUsage, KETL_SIZE_64B, KETL_REG_AX);
            __attribute__ ((fallthrough));
        }
        case KETL_IR_CODE_RETURN: {
            if (stackUsage > 0) {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x81, 0xc4, 0x00, 0x00, 0x00, 0x00,              // add     rsp, 0
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                *(int32_t*)(opcodesBuffer + length + 3) = (int32_t)stackUsage;
                length += sizeof(opcodesArray);
            }
            {
                const uint8_t opcodesArray[] =
                {
                    0xc3                    // ret
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            break;
        }
        default:
            KETL_DEBUGBREAK();
        }

        ketl_ir_operation* mainNext = itOperation[i].mainNext;
        if (mainNext != NULL && mainNext != &itOperation[i + 1]) {
            const uint8_t opcodesArray[] =
            {
                0xe9, 0xff, 0xff, 0x00, 0x00,                 // jmp .65535
            };
            memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));

            JumpInfo jumpInfo;
            jumpInfo.bufferOffset = length + 1;
            length += sizeof(opcodesArray);
            jumpInfo.fromOffset = length;
            jumpInfo.to = itOperation[i].mainNext;
            *(JumpInfo*)ketl_stack_push(&irCompiler->jumpList) = jumpInfo;
        }
    }

    {
        ketl_stack_iterator jumpIterator;
        ketl_stack_iterator_init(&jumpIterator, &irCompiler->jumpList);
        while (ketl_stack_iterator_has_next(&jumpIterator)) {
            JumpInfo* jumpInfo = ketl_stack_iterator_get_next(&jumpIterator);
            uint64_t* toOffset = ketl_int_map_get(&irCompiler->operationBufferOffsetMap, (ketl_int_map_key)jumpInfo->to);
            *(int32_t*)(opcodesBuffer + jumpInfo->bufferOffset) = (int32_t)(*toOffset - jumpInfo->fromOffset);
        }
    }

    const uint8_t* opcodes = opcodesBuffer;

    /*
    for (uint64_t i = 0; i < length; ++i) {
        printf("%.2X ", opcodes[i]);
    }
    printf("\n");
    //*/

    return (ketl_function*)ketl_executable_memory_allocate(&irCompiler->exeMemory, &opcodes, sizeof(opcodesBuffer), length);
}

void ketl_ir_compiler_init(ketl_ir_compiler* irCompiler) {
    ketl_executable_memory_init(&irCompiler->exeMemory);
    ketl_int_map_init(&irCompiler->operationBufferOffsetMap, sizeof(uint64_t), 16);
    ketl_stack_init(&irCompiler->jumpList, sizeof(JumpInfo), 16);
}

void ketl_ir_compiler_deinit(ketl_ir_compiler* irCompiler) {
    ketl_stack_deinit(&irCompiler->jumpList);
    ketl_int_map_deinit(&irCompiler->operationBufferOffsetMap);
    ketl_executable_memory_deinit(&irCompiler->exeMemory);
}