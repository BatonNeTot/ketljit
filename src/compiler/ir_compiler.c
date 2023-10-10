//🍲ketl
#include "compiler/ir_compiler.h"

#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
#include "ketl/function.h"
#include "ketl/type.h"

#include <string.h>

#include <stdio.h>

static uint64_t addIntLiteralIntoRax(uint8_t* buffer, int64_t value) {
    const uint8_t opcodesArray[] =
    {
        0x48, 0x01, 0xd8,   // add rax, rbx                             
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
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

static uint64_t loadIntLiteralIntoRbx(uint8_t* buffer, int64_t value) {
    const uint8_t opcodesArray[] =
    {
        0x48, 0xc7, 0xc3, 0xff, 0xff, 0x00, 0x00,  // mov rbx, 65535                             
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
    *(int32_t*)(buffer + 3) = (int32_t)value;
    return sizeof(opcodesArray);
}

static uint64_t loadArgumentIntoRax(uint8_t* buffer, KETLIRArgument* argument) {
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

static uint64_t loadArgumentIntoRbx(uint8_t* buffer, KETLIRArgument* argument) {
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_STACK: {
        const uint8_t opcodesArray[] =
        {
             0x48, 0x8b, 0x9c, 0x24, 0xff, 0xff, 0x00, 0x00,              // mov     rbx, QWORD PTR [rsp + 65535] 
        };
        memcpy(buffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(buffer + 4) = (int32_t)argument->stack;
        return sizeof(opcodesArray);
    }
    case KETL_IR_ARGUMENT_TYPE_INT8:
        return loadIntLiteralIntoRbx(buffer, argument->int8);
    case KETL_IR_ARGUMENT_TYPE_INT16:
        return loadIntLiteralIntoRbx(buffer, argument->int16);
    case KETL_IR_ARGUMENT_TYPE_INT32:
        return loadIntLiteralIntoRbx(buffer, argument->int32);
    case KETL_IR_ARGUMENT_TYPE_INT64:
        return loadIntLiteralIntoRbx(buffer, argument->int64);
    }
    __debugbreak();
    return 0;
}

static uint64_t loadRaxIntoArgument(uint8_t* buffer, KETLIRArgument* argument) {
    switch (argument->type) {
    case KETL_IR_ARGUMENT_TYPE_RETURN:
        return 0;
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

KETL_DEFINE(JumpInfo) {
    uint64_t bufferOffset;
    uint64_t fromOffset;
    KETLIROperation* to;
};


KETLFunction* ketlCompileIR(KETLIRCompiler* irCompiler, KETLIRFunction* irFunction) {
    ketlIntMapReset(&irCompiler->operationBufferOffsetMap);
    ketlResetStack(&irCompiler->jumpList);

    const uint64_t shadowSpaceSize = 32;
    const uint64_t stackUsage = irFunction->stackUsage + shadowSpaceSize;

    uint8_t opcodesBuffer[4096];
    uint64_t length = 0;

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
        case KETL_IR_CODE_ADD_INT64: {
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]);
            length += loadArgumentIntoRbx(opcodesBuffer + length, itOperation[i].arguments[2]);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x01, 0xd8,   // add rax, rbx                             
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                length += sizeof(opcodesArray);
            }
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_EQUAL_INT64: {
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]);
            length += loadArgumentIntoRbx(opcodesBuffer + length, itOperation[i].arguments[2]);
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x39, 0xd8,               // cmp     rax, rbx
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                *(int32_t*)(opcodesBuffer + length + 4) = (int32_t)itOperation[i].arguments[1]->stack;
                length += sizeof(opcodesArray);
            }
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
        case KETL_IR_CODE_ASSIGN_8_BYTES: {
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]);
            length += loadRaxIntoArgument(opcodesBuffer + length, itOperation[i].arguments[0]);
            break;
        }
        case KETL_IR_CODE_JUMP_IF_FALSE: {
            length += loadArgumentIntoRax(opcodesBuffer + length, itOperation[i].arguments[0]);
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
    for (uint64_t i = 0; i < length; ++i) {
        printf("%.2X ", opcodes[i]);
    }
    printf("\n");
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