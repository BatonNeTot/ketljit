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

	class Variable;

	namespace {
		template <class R, class... Args>
		class __VariableCallHelper {};

		template <class... Args>
		class __VariableCallHelper<void, Args...> {
		private:
			static void callVariable(ketl_variable* variable, Args... args) {
				ketl_variable_call_void(variable, args...);
			}

			friend Variable;
		};

		template <class... Args>
		class __VariableCallHelper<int64_t, Args...> {
		private:
			static int64_t callVariable(ketl_variable* variable, Args... args) {
				return ketl_variable_call_i64(variable, args...);
			}

			friend Variable;
		};

		template <class T>
		class __VariableGetHelper {};

		template <>
		class __VariableGetHelper<int64_t> {
		private:
			static int64_t getVariableAs(ketl_variable* variable) {
				return ketl_variable_get_as_i64(variable);
			}

			friend Variable;
		};
	}

	class Variable {
	public:
		Variable(const Variable& other) = delete; // TODO implement copz function for copy constructor
		Variable(Variable&& other) : _variableImpl(other._variableImpl) {
			other._variableImpl = nullptr;
		}
		~Variable() {
			ketl_variable_free(_variableImpl);
		}

		template <class R, class... Args>
		R call(Args... args) {
			return __VariableCallHelper<R, Args...>::callVariable(_variableImpl, args...);
		}

		template <class T>
		T as() {
			return __VariableGetHelper<T>::getVariableAs(_variableImpl);
		}

		explicit operator bool() {
			return _variableImpl;
		}

	private:
		Variable(ketl_variable* variable) : _variableImpl(variable) {}

		friend State;

		ketl_variable* _variableImpl;
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

		template <class R>
		Variable defineFunction(const char* name, ketl_type_pointer type, R(**function)(void*)) {
			return ketl_state_define_external_variable(_stateImpl, name, type, reinterpret_cast<void*>(function));
		}

		template <class R, class... Args>
		Variable defineFunction(const char* name, ketl_type_pointer type, R(**function)(void*, Args...)) {
			return ketl_state_define_external_variable(_stateImpl, name, type, reinterpret_cast<void*>(function));
		}

		template<class T>
		T& defineVariable(const char* name) {
			return defineVariable(name, T());
		}

		Variable compile(const std::string &source) {
			return ketl_state_compile_function(_stateImpl, source.c_str(), nullptr, 0);
		}

		Variable compile(const std::string &source, ketl_function_parameter* parameters, uint64_t parametersCount) {
			return ketl_state_compile_function(_stateImpl, source.c_str(), parameters, parametersCount);
		}

		Variable eval(const std::string &source) {
			return ketl_state_eval(_stateImpl, source.c_str());
		}

		Variable evalLocal(const std::string &source) {
			return ketl_state_eval_local(_stateImpl, source.c_str());
		}

		ketl_type_pointer i32() {
			return ketl_state_get_type_i32(_stateImpl);
		}

		ketl_type_pointer i64() {
			return ketl_state_get_type_i64(_stateImpl);
		}

		ketl_type_pointer getFunctionType(ketl_variable_features output, ketl_variable_features* parameters, uint64_t parameterCount) {
			return ketl_state_get_type_function(_stateImpl, output, parameters, parameterCount);
		}

	private:
		ketl_state* _stateImpl;
	};

}

#endif // ketl_ketl_hpp
