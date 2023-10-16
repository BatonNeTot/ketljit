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
	launchCheckTests();
#ifdef NDEBUG
	launchSpeedTests(10000000);
#else
	launchSpeedTests(10000);
#endif

	auto source = R"(
	test := a * b;
)";

	for (uint64_t i = 0; i < 1; ++i) {
		KETL::State ketlState;

		int64_t testVariable = 4;
		ketlState.defineVariable("test", testVariable);

		std::vector<KETLParameter> parameters = { 
			{"a", ketlState.i64(), KETLVariableTraits{false, false, KETL_TRAIT_TYPE_RVALUE}},
			{"b", ketlState.i64(), KETLVariableTraits{false, false, KETL_TRAIT_TYPE_RVALUE}},
		};
		KETL::Function function = ketlState.compile(source, parameters.data(), parameters.size());

		function.callVoid(2, 3);

		std::cout << testVariable << std::endl;

		testVariable = 5;
		function.callVoid(10, 4);

		std::cout << testVariable << std::endl;
	}

	return 0;
}