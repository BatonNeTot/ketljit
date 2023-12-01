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

	launchCheckTests();
#ifdef NDEBUG
	launchSpeedTests(10000000);
#else
	launchSpeedTests(10000);
#endif

	auto source = R"(
	if (test == 4) {
		i32 tmp := 80;
		return tmp;
	} else {
		return test;
	}
)";

	KETL::State ketlState;

	int64_t testVariable = 4;
	ketlState.defineVariable("test", testVariable);

	std::vector<KETLParameter> parameters = { 
		//{"a", ketlState.i32(), KETLVariableTraits{false, false, KETL_TRAIT_TYPE_RVALUE}},
		//{"b", ketlState.i32(), KETLVariableTraits{false, false, KETL_TRAIT_TYPE_RVALUE}},
	};
	KETL::Function function = ketlState.compile(source, parameters.data(), parameters.size());
	if (!function) {
		std::cerr << "Can't compile source string" << std::endl;
		return 0;
	}

	std::cout << function.call<int64_t>(2, 3) << std::endl;
	std::cout << testVariable << std::endl;

	testVariable = 5;

	std::cout << function.call<int64_t>(10, 4) << std::endl;
	std::cout << testVariable << std::endl;

	return 0;
}