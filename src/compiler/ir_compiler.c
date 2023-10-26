﻿//🍲ketl
#include "compiler/ir_compiler.h"

#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
#include "ketl/function.h"
#include "ketl/type.h"

#include <string.h>

#include <stdio.h>

static uint64_t loadIntLiteralIntoEax(uint8_t* buffer, int32_t value) {
    const uint8_t opcodesArray[] =
    {
        0xb8, 0xff, 0xff, 0x00, 0x00,   // mov eax, 65535                             
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
    *(int32_t*)(buffer + 1) = value;
    return sizeof(opcodesArray);
}

static uint64_t loadIntLiteralIntoRax(uint8_t* buffer, int64_t value) {
    const uint8_t opcodesArray[] =
    {
        0x48, 0xc7, 0xc0, 0xff, 0xff, 0x00, 0x00,  // mov rax, 65535                             
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
    *(int32_t*)(buffer + 3) = (int32_t)value;
    return sizeof(opcodesArray);
}

static uint64_t loadIntLiteralIntoEcx(uint8_t* buffer, int32_t value) {
    const uint8_t opcodesArray[] =
    {
        0xb9, 0xff, 0xff, 0x00, 0x00,  // mov ecx, 65535                             
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
    *(int32_t*)(buffer + 1) = value;
    return sizeof(opcodesArray);
}

static uint64_t loadIntLiteralIntoRcx(uint8_t* buffer, int64_t value) {
    const uint8_t opcodesArray[] =
    {
        0x48, 0xc7, 0xc1, 0xff, 0xff, 0x00, 0x00,  // mov rcx, 65535                             
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
    *(int32_t*)(buffer + 3) = (int32_t)value;
    return sizeof(opcodesArray);
}

static uint64_t loadArgumentIntoEax(uint8_t* buffer, KETLIRArgument* argument, uint64_t stackUsage) {
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: {
        const uint8_t opcodesArray[] =
        {
             0x8b, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     eax, DWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 3) = (int32_t)argument->stack;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES: {
        const uint8_t opcodesArray[] =
        {
             0x8b, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     eax, DWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 3) = (int32_t)(stackUsage + sizeof(void*) + argument->stack);
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_POINTER: {
        const uint8_t opcodesArray[] =
        {
             0xa1, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,         // mov     eax, DWORD PTR [65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(uint64_t*)(buffer + 1) = (uint64_t)argument->pointer;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_INT8:
        return loadIntLiteralIntoEax(buffer, argument->int8);
    case KETL_IR_ARGUMENT_TYPE_INT16:
        return loadIntLiteralIntoEax(buffer, argument->int16);
    case KETL_IR_ARGUMENT_TYPE_INT32:
        return loadIntLiteralIntoEax(buffer, argument->int32);
    }
    __debugbreak();
    return 0;
}

static uint64_t loadArgumentIntoRax(uint8_t* buffer, KETLIRArgument* argument, uint64_t stackUsage) {
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0x8b, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     rax, QWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 4) = (int32_t)argument->stack;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_8_BYTES: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0x8b, 0x84, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     rax, QWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 4) = (int32_t)(stackUsage + sizeof(void*) + argument->stack);
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_POINTER: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0xa1, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,         // mov     rax, QWORD PTR [65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(uint64_t*)(buffer + 2) = (uint64_t)argument->pointer;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_INT8:
        return loadIntLiteralIntoRax(buffer, argument->int8);
    case KETL_IR_ARGUMENT_TYPE_INT16:
        return loadIntLiteralIntoRax(buffer, argument->int16);
    case KETL_IR_ARGUMENT_TYPE_INT32:
        return loadIntLiteralIntoRax(buffer, argument->int32);
    case KETL_IR_ARGUMENT_TYPE_INT64:
        return loadIntLiteralIntoRax(buffer, argument->int64);
    }
    __debugbreak();
    return 0;
}

static uint64_t loadArgumentIntoEcx(uint8_t* buffer, KETLIRArgument* argument, uint64_t stackUsage) {
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: {
        const uint8_t opcodesArray[] =
        {
             0x8b, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     ecx, DWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 3) = (int32_t)argument->stack;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES: {
        const uint8_t opcodesArray[] =
        {
             0x8b, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     ecx, DWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 3) = (int32_t)(stackUsage + sizeof(void*) + argument->stack);
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_POINTER: {
        const uint8_t opcodesArray[] =
        {
             0xa1, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,         // mov     eax, DWORD PTR [65535] 
             0x89, 0xc1,                                                   // mov     ecx, eax 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(uint64_t*)(buffer + 1) = (uint64_t)argument->pointer;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_INT8:
        return loadIntLiteralIntoEcx(buffer, argument->int8);
    case KETL_IR_ARGUMENT_TYPE_INT16:
        return loadIntLiteralIntoEcx(buffer, argument->int16);
    case KETL_IR_ARGUMENT_TYPE_INT32:
        return loadIntLiteralIntoEcx(buffer, argument->int32);
    }
    __debugbreak();
    return 0;
}

static uint64_t loadArgumentIntoRcx(uint8_t* buffer, KETLIRArgument* argument, uint64_t stackUsage) {
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0x8b, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     rcx, QWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 4) = (int32_t)argument->stack;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_ARGUMENT_8_BYTES: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0x8b, 0x8c, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     rcx, QWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 4) = (int32_t)(stackUsage + sizeof(void*) + argument->stack);
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_POINTER: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0xa1, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,         // mov     rax, QWORD PTR [65535] 
             0x48, 0x89, 0xc1,                                                   // mov     rcx, rax 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(uint64_t*)(buffer + 2) = (uint64_t)argument->pointer;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_INT8:
        return loadIntLiteralIntoRcx(buffer, argument->int8);
    case KETL_IR_ARGUMENT_TYPE_INT16:
        return loadIntLiteralIntoRcx(buffer, argument->int16);
    case KETL_IR_ARGUMENT_TYPE_INT32:
        return loadIntLiteralIntoRcx(buffer, argument->int32);
    case KETL_IR_ARGUMENT_TYPE_INT64:
        return loadIntLiteralIntoRcx(buffer, argument->int64);
    }
    __debugbreak();
    return 0;
}

static uint64_t loadEaxIntoArgument(uint8_t* buffer, KETLIRArgument* argument) {
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
    __debugbreak();
    return 0;
}

static uint64_t loadRaxIntoArgument(uint8_t* buffer, KETLIRArgument* argument) {
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
    __debugbreak();
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


static uint64_t copyArgumentsToStack(uint8_t* buffer, KETLIRFunction* irFunction) {
    uint64_t length = 0;
    KETLIROperation* itOperation = irFunction->operations;
    KETLIRArgument* functionArgument = irFunction->arguments;
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
    KETLIROperation* to;
};


KETLFunction* ketlCompileIR(KETLIRCompiler* irCompiler, KETLIRFunction* irFunction) {
    ketlIntMapReset(&irCompiler->operationBufferOffsetMap);
    ketlResetStack(&irCompiler->jumpList);

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

    KETLIROperation* itOperation = irFunction->operations;
    for (uint64_t i = 0; i < irFunction->operationsCount; ++i) {
        {
            uint64_t* offset;
            ketlIntMapGetOrCreate(&irCompiler->operationBufferOffsetMap, (KETLIntMapKey)&itOperation[i], &offset);
            *offset = length;
        }

        switch (itOperation[i].code) {
        case KETL_IR_CODE_CAST_INT32_INT64: {
            length += loadArgumentIntoEax(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage);
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
            length += loadArgumentIntoEcx(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage);
            length += loadArgumentIntoEax(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage);
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
            length += loadArgumentIntoRcx(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage);
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage);
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
        case KETL_IR_CODE_MULTY_INT32: {
            length += loadArgumentIntoEcx(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage);
            length += loadArgumentIntoEax(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage);
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
            length += loadArgumentIntoRcx(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage);
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage);
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
            length += loadArgumentIntoRcx(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage);
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage);
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
            length += loadArgumentIntoRcx(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage);
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage);
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
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[2], stackUsage);
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[1]);
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_COPY_8_BYTES: {
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[1], stackUsage);
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_JUMP_IF_FALSE: {
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[0], stackUsage);
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
                *(JumpInfo*)ketlPushOnStack(&irCompiler->jumpList) = jumpInfo;
            }
            break;
        }
        case KETL_IR_CODE_RETURN_8_BYTES: {
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[0], stackUsage);
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
            __debugbreak();
        }

        KETLIROperation* mainNext = itOperation[i].mainNext;
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
            *(JumpInfo*)ketlPushOnStack(&irCompiler->jumpList) = jumpInfo;
        }
    }

    {
        KETLStackIterator jumpIterator;
        ketlInitStackIterator(&jumpIterator, &irCompiler->jumpList);
        while (ketlIteratorStackHasNext(&jumpIterator)) {
            JumpInfo* jumpInfo = ketlIteratorStackGetNext(&jumpIterator);
            uint64_t* toOffset = ketlIntMapGet(&irCompiler->operationBufferOffsetMap, (KETLIntMapKey)jumpInfo->to);
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

    return (KETLFunction*)ketlExecutableMemoryAllocate(&irCompiler->exeMemory, &opcodes, sizeof(opcodesBuffer), length);
}

void ketlIRCompilerInit(KETLIRCompiler* irCompiler) {
    ketlInitExecutableMemory(&irCompiler->exeMemory);
    ketlInitIntMap(&irCompiler->operationBufferOffsetMap, sizeof(uint64_t), 16);
    ketlInitStack(&irCompiler->jumpList, sizeof(JumpInfo), 16);
}

void ketlIRCompilerDeinit(KETLIRCompiler* irCompiler) {
    ketlDeinitStack(&irCompiler->jumpList);
    ketlDeinitIntMap(&irCompiler->operationBufferOffsetMap);
    ketlDeinitExecutableMemory(&irCompiler->exeMemory);
}