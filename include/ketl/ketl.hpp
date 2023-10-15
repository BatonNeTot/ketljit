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
			ketlDefineVariable(&_stateImpl, name, i64(), &variable);
		}

		Function compile(const char* source) {
			return ketlCompileFunction(&_stateImpl, source, nullptr, 0);
		}

		Function compile(const char* source, KETLParameter* parameters, uint64_t parametersCount) {
			return ketlCompileFunction(&_stateImpl, source, parameters, parametersCount);
		}

		KETLTypePtr i64() {
			return KETLTypePtr{ reinterpret_cast<KETLTypeBase*>(& _stateImpl.primitives.i64_t) };
		}

	private:
		KETLState _stateImpl;
	};

}

#endif /*ketl_h*/
