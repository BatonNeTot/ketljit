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

		void call() {
			using func_t = void(*)();
			reinterpret_cast<func_t>(_functionImpl)();
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
			return ketlCompileFunction(&_stateImpl, source);
		}

	private:
		KETLState _stateImpl;
	};

}

#endif /*ketl_h*/
