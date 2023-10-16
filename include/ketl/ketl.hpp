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

		template <class R, class... Args>
		R call(Args... args) {
			return KETL_CALL_FUNCTION(_functionImpl, R(*)(Args...), args...);
		}

		template <class... Args>
		void callVoid(Args... args) {
			KETL_CALL_FUNCTION(_functionImpl, void(*)(Args...), args...);
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
