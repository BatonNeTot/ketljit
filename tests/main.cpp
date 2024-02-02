/*🍲Ketl🍲*/
#include "check_tests.h"
#include "speed_tests.h"

#include "ketl/ketl.hpp"

#include <iostream>
#include <vector>
#include <string>

#include <cstdlib>

// TODO rethink iterators using int map iterator as an example
// TODO rethink getOrCreate in int map

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

#if 1
	launchCheckTests();
#ifdef NDEBUG
	launchSpeedTests(10000000);
#else
	launchSpeedTests(10000);
#endif
#endif

	auto source1 = R"(
		i64 inside := 7 + 6;
		return inside;
)";
	(void)source1;

	auto source2 = R"(
		i64 sum := a + b;
		if (sum != 5) {
			test := sum;
		}
		return sum;
)";

	for (auto i = 0u; i < 1; ++i) {
		KETL::State ketlState;
		
		std::cout << (ketlState.eval(source1) == 13) << std::endl;

		std::cout << (ketlState.eval("return inside + 22;") == 35) << std::endl;

		int64_t& testVariable = ketlState.defineVariable("test", 4l);

		std::vector<ketl_function_parameter> parameters = { 
			{"a", ketlState.i32(), ketl_variable_traits{false, false}},
			{"b", ketlState.i32(), ketl_variable_traits{false, false}},
		};
		KETL::Function function = ketlState.compileFunction(source2, parameters.data(), parameters.size());
		if (!function) {
			std::cerr << "Can't compile source string" << std::endl;
			return 0;
		}

		{
			auto result = function.call<int64_t>(2, 3);
			std::cout << (result == 5) << std::endl;
			std::cout << (testVariable == 4) << std::endl;
		}

		{
			auto result = function.call<int64_t>(10, 4);
			std::cout << (result == 14) << std::endl;
			std::cout << (testVariable == 14) << std::endl;
		}
	}

	return 0;
}