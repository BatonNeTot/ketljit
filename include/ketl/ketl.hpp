//🍲ketl
#ifndef ketl_hpp
#define ketl_hpp

extern "C" {
#include "ketl/ketl.h"
#include "ketl/function.h"
}

#include <iostream>

namespace KETL {

	class State;

	class Function {
	public:

		int64_t operator()() {
			int64_t result;
			ketlCallFunction(_functionImpl, &result);
			return result;
		}

		int64_t operator()(int64_t argument) {
			int64_t result;
			ketlCallFunctionWithArgument(_functionImpl, &result, argument);
			return result;
		}

		explicit operator bool() {
			return _functionImpl;
		}

	private:
		Function(KETLFunction* function) : _functionImpl(function) {}

		friend State;

		KETLFunction* _functionImpl;
	};

	class State {
	public:

		State() {
			ketlInitState(&_stateImpl);
		}

		~State() {
			ketlDeinitState(&_stateImpl);
		}

		void defineVariable(const char* name, int64_t& variable) {
			ketlDefineVariable(&_stateImpl, name, _stateImpl.primitives.i64_t, &variable);
		}

		Function compile(const char* source) {
			return ketlCompileFunction(&_stateImpl, source, nullptr, nullptr);
		}

		Function compile(const char* source, KETLType* argumentType, const char* argumentName) {
			return ketlCompileFunction(&_stateImpl, source, argumentType, argumentName);
		}

		KETLType* i64() {
			return _stateImpl.primitives.i64_t;
		}

	private:
		KETLState _stateImpl;
	};

}

#endif /*ketl_h*/
