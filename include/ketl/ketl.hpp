//🫖ketl
#ifndef ketl_ketl_hpp
#define ketl_ketl_hpp

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
			if constexpr (sizeof...(Args) == 0) {
				return KETL_CALL_FUNCTION(_functionImpl, R);
			} else {
				return KETL_CALL_FUNCTION_ARGS(_functionImpl, R, args...);
			}
		}

		template <class... Args>
		void callVoid(Args... args) {
			if constexpr (sizeof...(Args) == 0) {
				return KETL_CALL_FUNCTION(_functionImpl, void);
			} else {
				return KETL_CALL_FUNCTION_ARGS(_functionImpl, void, args...);
			}
		}

		explicit operator bool() {
			return _functionImpl;
		}

	private:
		Function(ketl_function* function) : _functionImpl(function) {}

		friend State;

		ketl_function* _functionImpl;
	};

	class State {
	public:

		State()
			: _stateImpl(ketl_state_create()) {}

		~State() {
			ketl_state_destroy(_stateImpl);
		}

		void defineVariable(const char* name, int64_t& variable) {
			ketl_state_define_external_variable(_stateImpl, name, i64(), &variable);
		}

		int64_t& defineVariable(const char* name, int64_t&& initialValue) {
			int64_t& variable = *reinterpret_cast<int64_t*>(ketl_state_define_internal_variable(_stateImpl, name, i64()));
			variable = initialValue;
			return variable;
		}

		template<class T>
		T& defineVariable(const char* name) {
			return defineVariable(name, T());
		}

		Function compileFunction(const std::string &source) {
			return ketlCompileFunction(_stateImpl, source.c_str(), nullptr, 0);
		}

		Function compileFunction(const std::string &source, ketl_function_parameter* parameters, uint64_t parametersCount) {
			return ketlCompileFunction(_stateImpl, source.c_str(), parameters, parametersCount);
		}

		int64_t eval(const std::string &source) {
			return ketl_state_eval(_stateImpl, source.c_str());
		}

		ketl_type_pointer i32() {
			return ketl_state_get_type_i32(_stateImpl);
		}

		ketl_type_pointer i64() {
			return ketl_state_get_type_i64(_stateImpl);
		}

	private:
		ketl_state* _stateImpl;
	};

}

#endif // ketl_ketl_hpp
