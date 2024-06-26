﻿//🫖ketl
#include "ir_compiler.h"

#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
#include "function_impl.h"
#include "ketl/type.h"

#include <string.h>

#include <stdio.h>

#define KETL_SIZE_8B    1
#define KETL_SIZE_16B   2
#define KETL_SIZE_32B   4
#define KETL_SIZE_64B   8

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
#define KETL_ARG_RBP_MEM_DISP   3
#define KETL_ARG_IMM            4
#define KETL_ARG_IMM_MEM        5

#define KETL_OP_MOV     0

KETL_DEFINE(OpStruct) {
    union {
        struct {
            uint8_t firstArgType;
            uint8_t secondArgType;
        };
        uint16_t argTypesHash;
    };
    uint8_t size;
    uint16_t opCode;
    uint64_t firstArg;
    uint64_t secondArg;
};

#define REX_PREFIX 0b0100

KETL_DEFINE(REXByte) {
    uint8_t b : 1;
    uint8_t x : 1;
    uint8_t r : 1;
    uint8_t w : 1;
    uint8_t prefix : 4;
};

KETL_DEFINE(MODRMByte) {
    uint8_t rm : 3;
    uint8_t reg : 3;
    uint8_t mod : 2;
};

#define MODRM_MOD_IND       0b00
#define MODRM_MOD_IND_DIS8  0b01
#define MODRM_MOD_IND_DIS32 0b10
#define MODRM_MOD_DIR       0b11

KETL_DEFINE(SIBByte) {
    uint8_t base : 3;
    uint8_t index : 3;
    uint8_t scale : 2;
};

#define SIB_SCALE_1 0b00
#define SIB_SCALE_2 0b01
#define SIB_SCALE_4 0b10
#define SIB_SCALE_8 0b11

static uint64_t execMov(uint8_t* buffer, OpStruct* opStruct) {
    uint64_t size = 0;

    if (opStruct->size == KETL_SIZE_16B) {
        buffer[size++] = 0x66; // set 16-bit operand size
    }

    REXByte rex = {
        .prefix = REX_PREFIX,
        .w = 0, .r = 0, .x = 0, .b = 0};
    REXByte* pRex = NULL;

    if (opStruct->size == KETL_SIZE_64B) {
        // set 64-bit operand size
        rex.w = 1;
        buffer[size] = *(uint8_t*)(&rex);
        pRex = (REXByte*)&buffer[size++];
    } else if (opStruct->size == KETL_SIZE_8B &&
        (
            (
                (opStruct->firstArgType == KETL_ARG_REG ||
                opStruct->firstArgType == KETL_ARG_REG_MEM) &&
                (opStruct->firstArg >= KETL_REG_SP || 
                opStruct->firstArg <= KETL_REG_DI)
            ) ||
            (
                (opStruct->secondArgType == KETL_ARG_REG ||
                opStruct->secondArgType == KETL_ARG_REG_MEM) &&
                (opStruct->secondArg >= KETL_REG_SP || 
                opStruct->secondArg <= KETL_REG_DI)
            )
        )) {
        // for accessing spl, bpl, sil and dil rex be inserted
        buffer[size] = *(uint8_t*)(&rex);
        pRex = (REXByte*)&buffer[size++];
    } else if (
        (
            (opStruct->firstArgType == KETL_ARG_REG ||
            opStruct->firstArgType == KETL_ARG_REG_MEM) &&
            opStruct->firstArg >= KETL_REG_R8) ||
        (
            (opStruct->secondArgType == KETL_ARG_REG ||
            opStruct->secondArgType == KETL_ARG_REG_MEM) &&
            opStruct->secondArg >= KETL_REG_R8
        )) {
        // for accessing r8-r15 registers
        // additional flags must be set, but they will be desided later
        buffer[size] = *(uint8_t*)(&rex);
        pRex = (REXByte*)&buffer[size++];
    }
    
    switch (opStruct->firstArgType) {
    case KETL_ARG_REG:
        switch (opStruct->secondArgType) {
        case KETL_ARG_REG:
        case KETL_ARG_REG_MEM:
        case KETL_ARG_RSP_MEM_DISP:
        case KETL_ARG_RBP_MEM_DISP:
            if (opStruct->size == KETL_SIZE_8B) {
                buffer[size++] = 0x8a;
            } else {
                buffer[size++] = 0x8b;
            }
            if (opStruct->firstArg >= KETL_REG_R8) {
                opStruct->firstArg -= KETL_REG_R8;
                pRex->r = 1;
            }
            if ((opStruct->secondArgType == KETL_ARG_REG ||
                opStruct->secondArgType == KETL_ARG_REG_MEM) &&
                opStruct->secondArg >= KETL_REG_R8) {
                opStruct->secondArg -= KETL_REG_R8;
                pRex->b = 1;
            }
            
            if (opStruct->secondArgType == KETL_ARG_REG) {
                // reg
                MODRMByte modrm = {
                    .mod = MODRM_MOD_DIR,
                    .reg = opStruct->firstArg,
                    .rm = opStruct->secondArg
                };
                buffer[size++] = *(uint8_t*)&modrm;
            } else {
                if (opStruct->secondArgType == KETL_ARG_REG_MEM) {
                    // [reg]
                    MODRMByte modrm = {
                        .mod = MODRM_MOD_IND,
                        .reg = opStruct->firstArg,
                        .rm = opStruct->secondArg
                    };
                    buffer[size++] = *(uint8_t*)&modrm;
                } else {
                    // [SIB]
                    MODRMByte modrm = {
                        .mod = MODRM_MOD_IND_DIS32,
                        .reg = opStruct->firstArg,
                        .rm = KETL_REG_SP
                    };
                    buffer[size++] = *(uint8_t*)&modrm;
                    // [RSP + disp32] or [RBP + disp32]
                    SIBByte sib = {
                        .scale =  SIB_SCALE_1,
                        .index = KETL_REG_SP,
                        .base = opStruct->secondArgType == KETL_ARG_RSP_MEM_DISP
                            ? KETL_REG_SP : KETL_REG_BP
                    };
                    buffer[size++] = *(uint8_t*)&sib;
                    *(uint32_t*)(buffer + size) = opStruct->secondArg;
                    size += 4;
                }
            }
            return size;
        case KETL_ARG_IMM:
            if (opStruct->size == KETL_SIZE_8B) {
                buffer[size++] = 0xb0 + opStruct->firstArg;
            } else {
                buffer[size++] = 0xb8 + opStruct->firstArg;
            }
            if (opStruct->firstArg >= KETL_REG_R8) {
                opStruct->firstArg -= KETL_REG_R8;
                pRex->r = 1;
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
    case KETL_ARG_REG_MEM:
    case KETL_ARG_RSP_MEM_DISP:
    case KETL_ARG_RBP_MEM_DISP:
        switch (opStruct->secondArgType) {
        case KETL_ARG_REG:
            if (opStruct->size == KETL_SIZE_8B) {
                buffer[size++] = 0x88;
            } else {
                buffer[size++] = 0x89;
            }
            if (opStruct->firstArgType == KETL_ARG_REG_MEM &&
                opStruct->firstArg >= KETL_REG_R8) {
                opStruct->firstArg -= KETL_REG_R8;
                pRex->b = 1;
            }
            if (opStruct->secondArg >= KETL_REG_R8) {
                opStruct->secondArg -= KETL_REG_R8;
                pRex->r = 1;
            }


            if (opStruct->firstArgType == KETL_ARG_REG_MEM) {
                // [reg]
                MODRMByte modrm = {
                    .mod = MODRM_MOD_IND,
                    .reg = opStruct->secondArg,
                    .rm = opStruct->firstArg
                };
                buffer[size++] = *(uint8_t*)&modrm;
            } else {
                // [SIB]
                MODRMByte modrm = {
                    .mod = MODRM_MOD_IND_DIS32,
                    .reg = opStruct->secondArg,
                    .rm = KETL_REG_SP
                };
                buffer[size++] = *(uint8_t*)&modrm;
                // [RSP + disp32] or [RBP + disp32]
                SIBByte sib = {
                    .scale =  SIB_SCALE_1,
                    .index = KETL_REG_SP,
                    .base = opStruct->firstArgType == KETL_ARG_RSP_MEM_DISP
                        ? KETL_REG_SP : KETL_REG_BP
                };
                buffer[size++] = *(uint8_t*)&sib;
                *(uint32_t*)(buffer + size) = opStruct->firstArg;
                size += 4;
            }
            return size;
        KETL_NODEFAULT()
        }
    case KETL_ARG_IMM_MEM:
        switch (opStruct->secondArgType) {
        case KETL_ARG_REG:
            if (opStruct->size == KETL_SIZE_8B) {
                buffer[size++] = 0xa2;
            } else {
                buffer[size++] = 0xa3;
            }
            *(uint64_t*)(buffer + size) = opStruct->firstArg;
            size += 8;
            if (opStruct->secondArg == KETL_REG_AX) {
                return size;
            } else {
                opStruct->firstArgType = KETL_ARG_REG;
                opStruct->firstArg = KETL_REG_AX;
                return size + execMov(buffer + size, opStruct);
            }
        KETL_NODEFAULT()
        }
    KETL_NODEFAULT()
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

#define EXEC_OPCODE_REG_RBP_DISP(buffer, sizeAccum, sizeArg, opcodeArg, regArg, immArg) do {\
    OpStruct tmpOpStruct;\
    tmpOpStruct.size = (sizeArg);\
    tmpOpStruct.firstArgType = KETL_ARG_REG;\
    tmpOpStruct.secondArgType = KETL_ARG_RBP_MEM_DISP;\
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

#define EXEC_OPCODE_RSP_DISP_REG(buffer, sizeAccum, sizeArg, opcodeArg, immArg, regArg) do {\
    OpStruct tmpOpStruct;\
    tmpOpStruct.size = (sizeArg);\
    tmpOpStruct.firstArgType = KETL_ARG_RSP_MEM_DISP;\
    tmpOpStruct.secondArgType = KETL_ARG_REG;\
    tmpOpStruct.opCode = (opcodeArg);\
    tmpOpStruct.firstArg = (immArg);\
    tmpOpStruct.secondArg = (regArg);\
    (sizeAccum) += execOpcode((buffer), &tmpOpStruct);\
} while(false)

#define EXEC_OPCODE_RBP_DISP_REG(buffer, sizeAccum, sizeArg, opcodeArg, immArg, regArg) do {\
    OpStruct tmpOpStruct;\
    tmpOpStruct.size = (sizeArg);\
    tmpOpStruct.firstArgType = KETL_ARG_RBP_MEM_DISP;\
    tmpOpStruct.secondArgType = KETL_ARG_REG;\
    tmpOpStruct.opCode = (opcodeArg);\
    tmpOpStruct.firstArg = (immArg);\
    tmpOpStruct.secondArg = (regArg);\
    (sizeAccum) += execOpcode((buffer), &tmpOpStruct);\
} while(false)

#define EXEC_OPCODE_IMM_MEM_REG(buffer, sizeAccum, sizeArg, opcodeArg, immArg, regArg) do {\
    OpStruct tmpOpStruct;\
    tmpOpStruct.size = (sizeArg);\
    tmpOpStruct.firstArgType = KETL_ARG_IMM_MEM;\
    tmpOpStruct.secondArgType = KETL_ARG_REG;\
    tmpOpStruct.opCode = (opcodeArg);\
    tmpOpStruct.firstArg = (immArg);\
    tmpOpStruct.secondArg = (regArg);\
    (sizeAccum) += execOpcode((buffer), &tmpOpStruct);\
} while(false)

static inline uint64_t loadArgumentIntoReg(uint8_t* buffer, ketl_ir_argument* argument, uint8_t sizeMacro, uint8_t regMacro) {
    uint64_t size = 0;
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: 
#if KETL_OS_WINDOWS
        EXEC_OPCODE_REG_RSP_DISP(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, (int32_t)argument->stack);
#else
        EXEC_OPCODE_REG_RBP_DISP(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, (int32_t)argument->stack);
#endif
        return size;
    case KETL_IR_ARGUMENT_TYPE_POINTER: 
        EXEC_OPCODE_REG_IMM(buffer, size, sizeMacro, KETL_OP_MOV, regMacro, (uint64_t)argument->pointer);
        return size;
    case KETL_IR_ARGUMENT_TYPE_REFERENCE: 
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
    return 0;
}

static uint64_t loadRegIntoArgument(uint8_t* buffer, ketl_ir_argument* argument, uint8_t sizeMacro, uint8_t regMacro) {
    uint8_t size = 0;
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: 
#if KETL_OS_WINDOWS
        EXEC_OPCODE_RSP_DISP_REG(buffer, size, sizeMacro, KETL_OP_MOV, (int32_t)argument->stack, regMacro);
#else
        EXEC_OPCODE_RBP_DISP_REG(buffer, size, sizeMacro, KETL_OP_MOV, (int32_t)argument->stack, regMacro);
#endif
        return size;
    case KETL_IR_ARGUMENT_TYPE_REFERENCE: 
        EXEC_OPCODE_IMM_MEM_REG(buffer, size, sizeMacro, KETL_OP_MOV, (uint64_t)argument->pointer, regMacro);
        return size;
    KETL_NODEFAULT()
    }
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


static uint8_t parameterRegs[] = {
#if !KETL_OS_WINDOWS
    KETL_REG_DI,
    KETL_REG_SI,
    KETL_REG_DX,
    KETL_REG_CX,
#else
    KETL_REG_CX,
    KETL_REG_DX,
#endif
    KETL_REG_R8,
    KETL_REG_R9,
};

#if KETL_OS_WINDOWS

void ketl_ir_compiler_adapt_ir(ketl_ir_function* irFunction, uint16_t parameterCount) {
    uint64_t stackUsage = irFunction->stackUsage = ((irFunction->stackUsage + 15) / 16) * 16; // 16 bites aligned
    ketl_ir_argument* functionArguments = irFunction->arguments;

    // first argument is silently functionClass
    ++parameterCount;
    uint16_t regParameterCount = parameterCount <= sizeof(parameterRegs) ? parameterCount : sizeof(parameterRegs);

    for (uint64_t i = 0; i < regParameterCount; ++i) {
        functionArguments[i].stack += sizeof(void*) + stackUsage;
    }
    for (uint64_t i = regParameterCount; i < parameterCount; ++i) {
        // TODO prepare stackParameters
    }
}

static uint64_t placeFunctionHeader(uint8_t* buffer, ketl_ir_function* irFunction, uint16_t parameterCount) {
    uint64_t length = 0;
    ketl_ir_argument* functionArguments = irFunction->arguments;
    uint64_t stackUsage = irFunction->stackUsage;

    // first argument is silently functionClass
    ++parameterCount;
    uint16_t regParameterCount = parameterCount <= sizeof(parameterRegs)? parameterCount : sizeof(parameterRegs);
    
    OpStruct movArgOpStruct;
    movArgOpStruct.firstArgType = KETL_ARG_RSP_MEM_DISP;
    movArgOpStruct.secondArgType = KETL_ARG_REG;
    movArgOpStruct.opCode = KETL_OP_MOV;

    // first argument is always functionClass
    movArgOpStruct.size = sizeof(void*);
    movArgOpStruct.firstArg = sizeof(void*);
    movArgOpStruct.secondArg = parameterRegs[0];
    length += execOpcode(buffer + length, &movArgOpStruct);

    for (uint64_t i = 1; i < regParameterCount; ++i) {
        movArgOpStruct.size = functionArguments[i - 1].stackSize;
        movArgOpStruct.firstArg = functionArguments[i - 1].stack - stackUsage;
        movArgOpStruct.secondArg = parameterRegs[i];
        length += execOpcode(buffer + length, &movArgOpStruct);
    }

    if (stackUsage > 0) {
        const uint8_t opcodesArray[] =
        {
            0x48, 0x81, 0xec, 0xff, 0xff, 0x00, 0x00,             // sub     rsp, 65535
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 3) = (int32_t)stackUsage;
        length += sizeof(opcodesArray);
    }

    return length;
}

static uint64_t placeFunctionFooter(uint8_t* buffer, uint64_t stackUsage) {
    uint64_t length = 0;
    if (stackUsage > 0) {
        const uint8_t opcodesArray[] =
        {
            0x48, 0x81, 0xc4, 0xff, 0xff, 0x00, 0x00,             // add     rsp, 65535
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 3) = (int32_t)stackUsage;
        length += sizeof(opcodesArray);
    }
    {
        const uint8_t opcodesArray[] =
        {
            0xc3                    // ret
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        length += sizeof(opcodesArray);
    }
    return length;
}

#else

void ketl_ir_compiler_adapt_ir(ketl_ir_function* irFunction, uint16_t parameterCount) {
    uint64_t stackUsage = irFunction->stackUsage;
    uint64_t functionArgumentCount = irFunction->argumentCount;
    ketl_ir_argument* functionArguments = irFunction->arguments;

    // first argument is silently functionClass
    ++parameterCount;
    uint16_t regParameterCount = parameterCount <= sizeof(parameterRegs) ? parameterCount : sizeof(parameterRegs);

    uint64_t regParametersSize;
    if (parameterCount > 1) {
        uint16_t lastRegParameterIndex = regParameterCount - 2;
        regParametersSize = functionArguments[lastRegParameterIndex].stack + functionArguments[lastRegParameterIndex].stackSize;
    } else {
        regParametersSize = sizeof(void*);
    }
    irFunction->stackUsage = ((stackUsage + regParametersSize + 15) / 16) * 16; // 16 bites aligned

    for (uint64_t i = 1; i < regParameterCount; ++i) {
        functionArguments[i - 1].stack += stackUsage + functionArguments[i - 1].stackSize;
        functionArguments[i - 1].stack *= -1;
    }
    for (uint64_t i = regParameterCount; i < parameterCount; ++i) {
        functionArguments[i - 1].stack = (i - regParameterCount + 2) * sizeof(void*);
    }
    for (uint64_t i = parameterCount - 1; i < functionArgumentCount; ++i) {
        if (functionArguments[i].type == KETL_IR_ARGUMENT_TYPE_STACK) {
            functionArguments[i].stack += functionArguments[i].stackSize;
            functionArguments[i].stack *= -1;
        }
    }
}

static uint64_t placeFunctionHeader(uint8_t* buffer, ketl_ir_function* irFunction, uint16_t parameterCount) {
    uint64_t length = 0;
    ketl_ir_argument* functionArguments = irFunction->arguments;
    uint64_t stackUsage = irFunction->stackUsage;

    {
        const uint8_t opcodesArray[] =
        {
            0x55,                   // push    rbp
            0x48, 0x89, 0xe5,       // mov     rbp, rsp
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        length += sizeof(opcodesArray);
    }

    if (stackUsage > 0) {
        const uint8_t opcodesArray[] =
        {
            0x48, 0x81, 0xec, 0xff, 0xff, 0x00, 0x00,             // sub     rsp, 65535
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + length + 3) = (int32_t)stackUsage;
        length += sizeof(opcodesArray);
    }

    // first argument is silently functionClass
    ++parameterCount;
    uint16_t regParameterCount = parameterCount < sizeof(parameterRegs) - 1 ? parameterCount : sizeof(parameterRegs) - 1;
    
    OpStruct movArgOpStruct;
    movArgOpStruct.firstArgType = KETL_ARG_RBP_MEM_DISP;
    movArgOpStruct.secondArgType = KETL_ARG_REG;
    movArgOpStruct.opCode = KETL_OP_MOV;

    // first argument is always functionClass
    movArgOpStruct.size = sizeof(void*);
    if (parameterCount <= 1) {
        // temp hack until functionClass would be a proper argument
        movArgOpStruct.firstArg = -(stackUsage + sizeof(void*));
    } else {
        movArgOpStruct.firstArg = functionArguments[0].stack + functionArguments[0].stackSize;
    }
    movArgOpStruct.secondArg = parameterRegs[0];
    length += execOpcode(buffer + length, &movArgOpStruct);

    for (uint64_t i = 1; i < regParameterCount; ++i) {
        movArgOpStruct.size = functionArguments[i - 1].stackSize;
        movArgOpStruct.firstArg = functionArguments[i - 1].stack;
        movArgOpStruct.secondArg = parameterRegs[i];
        length += execOpcode(buffer + length, &movArgOpStruct);
    }

    return length;
}

static uint64_t placeFunctionFooter(uint8_t* buffer, uint64_t stackUsage) {
    (void)stackUsage;
    uint64_t length = 0;
    if (stackUsage > 0) {
        const uint8_t opcodesArray[] =
        {
            0xc9,             // leave
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        length += sizeof(opcodesArray);
    } else {
        const uint8_t opcodesArray[] =
        {
            0x5d               // pop     rbp
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        length += sizeof(opcodesArray);
    }
    {
        const uint8_t opcodesArray[] =
        {
            0xc3                // ret
        };
        memcpy(buffer + length, opcodesArray, sizeof(opcodesArray));
        length += sizeof(opcodesArray);
    }
    return length;
}

#endif

KETL_DEFINE(JumpInfo) {
    uint64_t bufferOffset;
    uint64_t fromOffset;
    ketl_ir_operation* to;
};


const uint8_t* ketl_ir_compiler_compile(ketl_ir_compiler* irCompiler, ketl_ir_function* irFunction, uint16_t parameterCount) {
    ketl_int_map_reset(&irCompiler->operationBufferOffsetMap);
    ketl_stack_reset(&irCompiler->jumpList);

    uint64_t stackUsage = irFunction->stackUsage;

    uint8_t opcodesBuffer[4096];
    uint64_t length = 0;


    length = placeFunctionHeader(opcodesBuffer, irFunction, parameterCount);

    ketl_ir_operation* itOperation = irFunction->operations;
    uint64_t operationsCount = irFunction->operationCount;
    for (uint64_t i = 0; i < operationsCount; ++i) {
        {
            uint64_t* offset;
            ketl_int_map_get_or_create(&irCompiler->operationBufferOffsetMap, (ketl_int_map_key)&itOperation[i], &offset);
            *offset = length;
        }

        switch (itOperation[i].code) {
        case KETL_IR_CODE_NONE: break;
        case KETL_IR_CODE_CAST_INT32_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_32B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x98,   // cdqe // signed extending eax to rax                                
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_64B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_ADD_INT32: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], KETL_SIZE_32B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_32B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x01, 0xc8,   // add eax, ecx                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_32B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_ADD_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_64B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x01, 0xc8,   // add rax, rcx                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_64B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_SUB_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_64B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x29, 0xc8,    // sub rax, rcx                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_64B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_MULTY_INT32: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], KETL_SIZE_32B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_32B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0xf7, 0xe9,   // imul ecx // edx:eax = ecx * eax                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_32B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_MULTY_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_64B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0xf7, 0xe9,   // imul rcx // rdx:rax = rcx * rax                                   
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_64B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_EQUAL_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_64B, KETL_REG_AX);
            length += compareRaxAndRcx(opcodesBuffer + length);
            {
                const uint8_t opcodesArray[] =
                {
                    0x0f, 0x94, 0xc0,                                                               // sete al
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_8B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_UNEQUAL_INT64: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], KETL_SIZE_64B, KETL_REG_CX);
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_64B, KETL_REG_AX);
            length += compareRaxAndRcx(opcodesBuffer + length);
            {
                const uint8_t opcodesArray[] =
                {
                    0x0f, 0x95, 0xc0,                                                               // setne al
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_8B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_ASSIGN_8_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[2], KETL_SIZE_64B, KETL_REG_AX);
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_64B, KETL_REG_AX);
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_64B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_COPY_4_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_32B, KETL_REG_AX);
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_32B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_COPY_8_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_64B, KETL_REG_AX);
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_64B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_JUMP_IF_FALSE: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_8B, KETL_REG_AX);
            {
                const uint8_t opcodesArray[] =
                {
                    0x84, 0xc0,                                   // test al, al
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
        case KETL_IR_CODE_CALL: {
            // TODO load all arguments
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[1], KETL_SIZE_64B, parameterRegs[0]);
            {
                const uint8_t opcodesArray[] =
                {
                    0xff, 0x17,                              // call [parameterRegs[0]] (call [rdi] for now)
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRegIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_64B, KETL_REG_AX);
            break;
        }
        case KETL_IR_CODE_RETURN_4_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_32B, KETL_REG_AX);
            length += placeFunctionFooter(opcodesBuffer + length, stackUsage);
            break;
        }
        case KETL_IR_CODE_RETURN_8_BYTES: {
            length += loadArgumentIntoReg(opcodesBuffer + length, itOperation[i].arguments[0], KETL_SIZE_64B, KETL_REG_AX);
            length += placeFunctionFooter(opcodesBuffer + length, stackUsage);
            break;
        }
        case KETL_IR_CODE_RETURN: {
            length += placeFunctionFooter(opcodesBuffer + length, stackUsage);
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

#if 0
    for (uint64_t i = 0; i < length; ++i) {
        printf("%.2X ", opcodes[i]);
    }
    printf("\n");
#endif

    return ketl_executable_memory_allocate(&irCompiler->exeMemory, &opcodes, sizeof(opcodesBuffer), length);
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