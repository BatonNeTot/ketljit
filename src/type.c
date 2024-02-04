//🫖ketl
#include "type_impl.h"

#include "ketl_impl.h"

uint64_t ketl_type_get_stack_size(ketl_type_pointer type) {
	return type.base->kind == KETL_TYPE_KIND_PRIMITIVE ? type.primitive->size : sizeof(void*);
}

uint64_t ketl_type_get_size(ketl_type_pointer type) {
	switch (type.base->kind) {
		case KETL_TYPE_KIND_META: {
			return type.meta->size;
		}
		case KETL_TYPE_KIND_PRIMITIVE: {
			return type.primitive->size;
		}
		case KETL_TYPE_KIND_FUNCTION_CLASS: {
			return type.functionClass->size;
		}
		case KETL_TYPE_KIND_CLASS: {
			return type.clazz->size;
		}
	}
	return 0;
}

ketl_ir_argument_type get_ir_argument_type(ketl_state* state, ketl_type_pointer type) {
	if (type.base->kind != KETL_TYPE_KIND_PRIMITIVE) {
		return KETL_IR_ARGUMENT_TYPE_POINTER;
	}

	switch (type.primitive->size) {
	case 1: 
		if (type.primitive == &state->primitives.i8_t) {
			return KETL_IR_ARGUMENT_TYPE_INT8;
		} else /* if (type.primitive == &state->primitives.u8_t) */ {
			return KETL_IR_ARGUMENT_TYPE_UINT8;
		}
	case 2: 
		if (type.primitive == &state->primitives.i16_t) {
			return KETL_IR_ARGUMENT_TYPE_INT16;
		} else /* if (type.primitive == &state->primitives.u16_t) */ {
			return KETL_IR_ARGUMENT_TYPE_UINT16;
		}
	case 4:
		if (type.primitive == &state->primitives.i32_t) {
			return KETL_IR_ARGUMENT_TYPE_INT32;
		} else if (type.primitive == &state->primitives.u32_t) {
			return KETL_IR_ARGUMENT_TYPE_UINT32;
		} else /* if (type.primitive == &state->primitives.f32_t) */ {
			return KETL_IR_ARGUMENT_TYPE_FLOAT32;
		}
	case 8:
		if (type.primitive == &state->primitives.i64_t) {
			return KETL_IR_ARGUMENT_TYPE_INT64;
		} else if (type.primitive == &state->primitives.u64_t) {
			return KETL_IR_ARGUMENT_TYPE_UINT64;
		} else /* if (type.primitive == &state->primitives.f64_t) */ {
			return KETL_IR_ARGUMENT_TYPE_FLOAT64;
		}
	KETL_NODEFAULT()
	}

	return KETL_IR_ARGUMENT_TYPE_NONE;
}