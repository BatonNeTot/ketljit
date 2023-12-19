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
			ketl_state_define_external_variable(&_stateImpl, name, i64(), &variable);
		}

		int64_t& defineVariable(const char* name, int64_t&& initialValue) {
			int64_t& variable = *reinterpret_cast<int64_t*>(ketl_state_define_internal_variable(&_stateImpl, name, i64()));
			variable = initialValue;
			return variable;
		}

		template<class T>
		T& defineVariable(const char* name) {
			return defineVariable(name, T());
		}

		Function compile(const std::string &source) {
			return ketlCompileFunction(&_stateImpl, source.c_str(), nullptr, 0);
		}

		Function compile(const std::string &source, KETLParameter* parameters, uint64_t parametersCount) {
			return ketlCompileFunction(&_stateImpl, source.c_str(), parameters, parametersCount);
		}

		KETLTypePtr i32() {
			return KETLTypePtr{ reinterpret_cast<KETLTypeBase*>(&_stateImpl.primitives.i32_t) };
		}

		KETLTypePtr i64() {
			return KETLTypePtr{ reinterpret_cast<KETLTypeBase*>(& _stateImpl.primitives.i64_t) };
		}

	private:
		KETLState _stateImpl;
	};

}

#endif /*ketl_h*/
