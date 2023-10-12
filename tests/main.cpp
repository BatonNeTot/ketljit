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
	if (test == 4) {
		return;
	}
	test := 7;
)";


	for (uint64_t i = 0; i < 1; ++i) {
		KETL::State ketlState;

		int64_t testVariable = 4;
		ketlState.defineVariable("test", testVariable);

		KETL::Function function = ketlState.compile(source);

		function.call();

		std::cout << testVariable << std::endl;

		testVariable = 5;
		function.call();

		std::cout << testVariable << std::endl;
	}

	return 0;
}