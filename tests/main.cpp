//🫖ketl
#include "check_tests.h"
#include "speed_tests.h"

#include "ketl/ketl.hpp"

#include <iostream>
#include <vector>
#include <string>

#include <cstdlib>

// TODO rethink iterators using int map iterator as an example
// TODO rethink getOrCreate in int map

int64_t testGetter(void*) {
	return 42;
}

int64_t testSum(void*, int64_t a, int64_t b) {
	return a + b;
}

int main(int argc, char** argv) {
	(void)argc;
	(void)argv;

#if 0
	launchCheckTests();
#ifdef NDEBUG
	launchSpeedTests(10000000);
#else
	launchSpeedTests(10000);
#endif
#endif

	KETL::State ketlState;

/*
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
	
	std::cout << (ketlState.eval(source1).as<int64_t>() == 13) << std::endl;

	std::cout << (ketlState.eval("return inside + 22;").as<int64_t>() == 35) << std::endl;
*/

	ketl_variable_features output;
	output.type = ketlState.i64();
	output.traits.hash = 0;
	{
		auto function = &testGetter;
		auto functionClass = &function;
		KETL::Variable testVar = ketlState.defineFunction("testGetter", ketlState.getFunctionType(output, nullptr, 0), functionClass);
		std::cout << testVar.call<int64_t>() << std::endl;
		//ketlState.eval("testGetter();");
	}
	{
		auto function = &testSum;
		auto functionClass = &function;

		ketl_variable_features input[2];
		input[0].type = ketlState.i64();
		input[0].traits.hash = 0;
		input[1].type = ketlState.i64();
		input[1].traits.hash = 0;
		KETL::Variable testVar = ketlState.defineFunction("testSum", ketlState.getFunctionType(output, input, 2), functionClass);
		std::cout << testVar.call<int64_t>(5, 6) << std::endl;
		//ketlState.eval("testGetter(5, 6);");
	}
/*
	int64_t& testVariable = ketlState.defineVariable("test", 4l);

	std::vector<ketl_function_parameter> parameters = { 
		{"a", ketlState.i32(), ketl_variable_traits{false, false}},
		{"b", ketlState.i32(), ketl_variable_traits{false, false}},
	};
	KETL::Variable function = ketlState.compile(source2, parameters.data(), parameters.size());
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
*/
	return 0;
}