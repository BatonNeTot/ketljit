﻿//🍲ketl
#ifndef compiler_ir_node_h
#define compiler_ir_node_h

#include "ketl/utils.h"

typedef uint8_t KETLIRArgumentType;

#define KETL_IR_ARGUMENT_TYPE_NONE 0
#define KETL_IR_ARGUMENT_TYPE_STACK 1
#define KETL_IR_ARGUMENT_TYPE_POINTER 2

#define KETL_IR_ARGUMENT_TYPE_ARGUMENT_1_BYTE 3
#define KETL_IR_ARGUMENT_TYPE_ARGUMENT_2_BYTES 4
#define KETL_IR_ARGUMENT_TYPE_ARGUMENT_4_BYTES 5
#define KETL_IR_ARGUMENT_TYPE_ARGUMENT_8_BYTES 6

#define KETL_IR_ARGUMENT_TYPE_BOOL 7

#define KETL_IR_ARGUMENT_TYPE_INT8 8
#define KETL_IR_ARGUMENT_TYPE_INT16 9
#define KETL_IR_ARGUMENT_TYPE_INT32 10
#define KETL_IR_ARGUMENT_TYPE_INT64 11

#define KETL_IR_ARGUMENT_TYPE_UINT8 12
#define KETL_IR_ARGUMENT_TYPE_UINT16 13
#define KETL_IR_ARGUMENT_TYPE_UINT32 14
#define KETL_IR_ARGUMENT_TYPE_UINT64 15

#define KETL_IR_ARGUMENT_TYPE_FLOAT32 16
#define KETL_IR_ARGUMENT_TYPE_FLOAT64 17

KETL_DEFINE(KETLIRArgument) {
	union {
		uint64_t stack;
		void* pointer;

		bool boolean;

		int8_t int8;
		int16_t int16;
		int32_t int32;
		int64_t int64;
		
		uint8_t uint8;
		uint16_t uint16;
		uint32_t uint32;
		uint64_t uint64;
		
		float float32;
		double float64;
	};
	KETLIRArgumentType type;
};

typedef uint8_t KETLIROperationCode;



#define KETL_IR_CODE_NONE 0

#define KETL_IR_CODE_CAST_INT8_INT16 1
#define KETL_IR_CODE_CAST_INT8_INT32 2
#define KETL_IR_CODE_CAST_INT8_INT64 3
#define KETL_IR_CODE_CAST_INT8_FLOAT32 4
#define KETL_IR_CODE_CAST_INT8_FLOAT64 5

#define KETL_IR_CODE_CAST_INT16_INT8 6
#define KETL_IR_CODE_CAST_INT16_INT32 7
#define KETL_IR_CODE_CAST_INT16_INT64 8
#define KETL_IR_CODE_CAST_INT16_FLOAT32 9
#define KETL_IR_CODE_CAST_INT16_FLOAT64 10

#define KETL_IR_CODE_CAST_INT32_INT8 11
#define KETL_IR_CODE_CAST_INT32_INT16 12
#define KETL_IR_CODE_CAST_INT32_INT64 13
#define KETL_IR_CODE_CAST_INT32_FLOAT32 14
#define KETL_IR_CODE_CAST_INT32_FLOAT64 15

#define KETL_IR_CODE_CAST_INT64_INT8 16
#define KETL_IR_CODE_CAST_INT64_INT16 17
#define KETL_IR_CODE_CAST_INT64_INT32 18
#define KETL_IR_CODE_CAST_INT64_FLOAT32 19
#define KETL_IR_CODE_CAST_INT64_FLOAT64 20

#define KETL_IR_CODE_CAST_FLOAT32_INT8 21
#define KETL_IR_CODE_CAST_FLOAT32_INT16 22
#define KETL_IR_CODE_CAST_FLOAT32_INT32 23
#define KETL_IR_CODE_CAST_FLOAT32_INT64 24
#define KETL_IR_CODE_CAST_FLOAT32_FLOAT64 25

#define KETL_IR_CODE_CAST_FLOAT64_INT8 26
#define KETL_IR_CODE_CAST_FLOAT64_INT16 27
#define KETL_IR_CODE_CAST_FLOAT64_INT32 28
#define KETL_IR_CODE_CAST_FLOAT64_INT64 29
#define KETL_IR_CODE_CAST_FLOAT64_FLOAT32 30

#define KETL_IR_CODE_ADD_INT8 31
#define KETL_IR_CODE_ADD_INT16 32
#define KETL_IR_CODE_ADD_INT32 33
#define KETL_IR_CODE_ADD_INT64 34
#define KETL_IR_CODE_ADD_FLOAT32 35
#define KETL_IR_CODE_ADD_FLOAT64 36

#define KETL_IR_CODE_SUB_INT8 37
#define KETL_IR_CODE_SUB_INT16 38
#define KETL_IR_CODE_SUB_INT32 39
#define KETL_IR_CODE_SUB_INT64 40
#define KETL_IR_CODE_SUB_FLOAT32 41
#define KETL_IR_CODE_SUB_FLOAT64 42

#define KETL_IR_CODE_MULTY_INT8 43
#define KETL_IR_CODE_MULTY_INT16 44
#define KETL_IR_CODE_MULTY_INT32 45
#define KETL_IR_CODE_MULTY_INT64 46
#define KETL_IR_CODE_MULTY_FLOAT32 47
#define KETL_IR_CODE_MULTY_FLOAT64 48

#define KETL_IR_CODE_DIV_INT8 49
#define KETL_IR_CODE_DIV_INT16 50
#define KETL_IR_CODE_DIV_INT32 51
#define KETL_IR_CODE_DIV_INT64 52
#define KETL_IR_CODE_DIV_FLOAT32 53
#define KETL_IR_CODE_DIV_FLOAT64 54

#define KETL_IR_CODE_EQUAL_INT8 55
#define KETL_IR_CODE_EQUAL_INT16 56
#define KETL_IR_CODE_EQUAL_INT32 57
#define KETL_IR_CODE_EQUAL_INT64 58
#define KETL_IR_CODE_EQUAL_FLOAT32 59
#define KETL_IR_CODE_EQUAL_FLOAT64 60

#define KETL_IR_CODE_UNEQUAL_INT8 61
#define KETL_IR_CODE_UNEQUAL_INT16 62
#define KETL_IR_CODE_UNEQUAL_INT32 63
#define KETL_IR_CODE_UNEQUAL_INT64 64
#define KETL_IR_CODE_UNEQUAL_FLOAT32 65
#define KETL_IR_CODE_UNEQUAL_FLOAT64 66

#define KETL_IR_CODE_ASSIGN_8_BYTES 67

#define KETL_IR_CODE_COPY_8_BYTES 68

#define KETL_IR_CODE_JUMP 69
#define KETL_IR_CODE_JUMP_IF_TRUE 70
#define KETL_IR_CODE_JUMP_IF_FALSE 71

#define KETL_IR_CODE_RETURN 72
#define KETL_IR_CODE_RETURN_8_BYTES 73

KETL_DEFINE(KETLIROperation) {
	KETLIROperationCode code;
	uint32_t argumentCount;
	KETLIRArgument** arguments;
	KETLIROperation* mainNext;
	KETLIROperation* extraNext;
};

KETL_DEFINE(KETLIRFunction) {
	uint64_t operationsCount;
	uint64_t stackUsage;
	KETLIROperation* operations;
	KETLIRArgument* arguments;
};

#endif /*compiler_ir_node_h*/
