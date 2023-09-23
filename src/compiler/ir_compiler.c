﻿//🍲ketl
#include "compiler/ir_compiler.h"

#include "executable_memory.h"
#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
#include "ketl/function.h"
#include "ketl/type.h"

#include <string.h>

#include <stdio.h>

static uint64_t addIntLiteralIntoRax(uint8_t* buffer, int64_t value) {
    const uint8_t opcodesArray[] =
    {
        0x48, 0x05, 0x00, 0x00, 0x00, 0x00,  // add rax, 0                             
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
    *(int32_t*)(buffer + 2) = (int32_t)value;
    return sizeof(opcodesArray);
}

static uint64_t loadIntLiteralIntoRax(uint8_t* buffer, int64_t value) {
    const uint8_t opcodesArray[] =
    {
        0x48, 0xc7, 0xc0, 0x00, 0x00, 0x00, 0x00,  // mov rax, 0                             
    };
    memcpy(buffer, opcodesArray, sizeof(opcodesArray));
    *(int32_t*)(buffer + 3) = (int32_t)value;
    return sizeof(opcodesArray);
}


KETLFunction* ketlCompileIR(KETLExecutableMemory* exeMemory, KETLIRFunction* irFunction) {
    uint8_t opcodesBuffer[4096];
    uint64_t length = 0;

    {
        const uint8_t opcodesArray[] =
        {
            0x48, 0x81, 0xec, 0x00, 0x00, 0x00, 0x00,             // sub     rsp, 0
        };
        memcpy(opcodesBuffer, opcodesArray, sizeof(opcodesArray));
        *(int32_t*)(opcodesBuffer + 3) = (int32_t)irFunction->stackUsage;
        length += sizeof(opcodesArray);
    }

    KETLIROperation* itOperation = irFunction->operations;
    for (uint64_t i = 0; i < irFunction->operationsCount; ++i) {

        switch (itOperation[i].code) {
        case KETL_IR_CODE_ADD_INT64: {
            switch (itOperation[i].arguments[1]->type) {
            case KETL_IR_ARGUMENT_TYPE_INT8:
                length += loadIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]->int8);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT16:
                length += loadIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]->int16);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT32:
                length += loadIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]->int32);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT64:
                length += loadIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]->int64);
                break;
            }
            switch (itOperation[i].arguments[2]->type) {
            case KETL_IR_ARGUMENT_TYPE_INT8:
                length += addIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[2]->int8);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT16:
                length += addIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[2]->int16);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT32:
                length += addIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[2]->int32);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT64:
                length += addIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[2]->int64);
                break;
            }
            switch (itOperation[i].arguments[0]->type) {
            case KETL_IR_ARGUMENT_TYPE_RETURN:
                break;
            case KETL_IR_ARGUMENT_TYPE_STACK: {
                const uint8_t opcodesArray[] =
                {
                     0x48, 0x89, 0x84, 0x24, 0x00, 0x00, 0x00, 0x00,              // mov     QWORD PTR [rsp + 0], rax
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                *(int32_t*)(opcodesBuffer + length + 4) = (int32_t)itOperation[i].arguments[0]->stack;
                length += sizeof(opcodesArray);
                break;
            }
            default:
                __debugbreak();
            }
            break;
        }
        case KETL_IR_CODE_ASSIGN_8_BYTES: {
            switch (itOperation[i].arguments[1]->type) {
            case KETL_IR_ARGUMENT_TYPE_STACK: {
                const uint8_t opcodesArray[] =
                {
                     0x48, 0x8b, 0x84, 0x24, 0x00, 0x00, 0x00, 0x00,              // mov     rax, QWORD PTR [rsp + 0] 
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                *(int32_t*)(opcodesBuffer + length + 4) = (int32_t)itOperation[i].arguments[1]->stack;
                length += sizeof(opcodesArray);
                break;
            }
            case KETL_IR_ARGUMENT_TYPE_INT8:
                length += loadIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]->int8);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT16:
                length += loadIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]->int16);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT32:
                length += loadIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]->int32);
                break;
            case KETL_IR_ARGUMENT_TYPE_INT64:
                length += loadIntLiteralIntoRax(opcodesBuffer + length, itOperation[i].arguments[1]->int64);
                break;
            }
            switch (itOperation[i].arguments[0]->type) {
            case KETL_IR_ARGUMENT_TYPE_RETURN:
                break;
            }
            break;
        }
        case KETL_IR_CODE_RETURN: {
            {
                const uint8_t opcodesArray[] =
                {
                    0x48, 0x81, 0xc4, 0x00, 0x00, 0x00, 0x00,              // add     rsp, 0
                };
                memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
                *(int32_t*)(opcodesBuffer + length + 3) = (int32_t)irFunction->stackUsage;
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
    }

    const uint8_t* opcodes = opcodesBuffer;
    for (uint64_t i = 0; i < length; ++i) {
        printf("%.2X ", opcodes[i]);
    }
    printf("\n");
    return (KETLFunction*)ketlExecutableMemoryAllocate(exeMemory, &opcodes, sizeof(opcodesBuffer), length);
}