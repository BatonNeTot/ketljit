//🍲ketl
#include "compiler/ir_compiler.h"

#include "executable_memory.h"
#include "compiler/ir_node.h"
#include "compiler/ir_builder.h"
#include "ketl/function.h"
#include "ketl/type.h"

#include <string.h>

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
    KETLIROperation* itOperation = irFunction->operations;
    for (uint64_t i = 0; i < irFunction->operationsCount; ++i) {

        switch (itOperation[i].code) {
        case KETL_IR_CODE_ASSIGN_8_BYTES: {
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
            switch (itOperation[i].arguments[0]->type) {
            case KETL_IR_ARGUMENT_TYPE_RETURN:
                break;
            }
            break;
        }
        case KETL_IR_CODE_RETURN: {
            const uint8_t opcodesArray[] =
            {
                0xc3                    // ret
            };
            memcpy(opcodesBuffer + length, opcodesArray, sizeof(opcodesArray));
            length += sizeof(opcodesArray);
            break;
        }
        }
    }

    const uint8_t* opcodes = opcodesBuffer;

    return (KETLFunction*)ketlExecutableMemoryAllocate(exeMemory, &opcodes, sizeof(opcodesBuffer), length);
}