/*🍲Ketl🍲*/
#include "check_tests.h"

static auto registerTests = []() {

	registerCheckTest("If else statement true", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", sum);

		auto compilationResult = state.compile(R"(
			sum := 3;

			if (sum == 3) {
				sum := 5;
			} else {
				sum := 7;
			}

			return 0;
		)").call<int64_t>();

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}*/

		return sum == 5;
		});

	registerCheckTest("If else statement false", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", sum);

		auto compilationResult = state.compile(R"(
			sum := 4;

			if (sum == 3) {
				sum := 5;
			} else {
				sum := 7;
			}

			return 0;
		)").call<int64_t>();

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}*/

		return sum == 7;
		});

	registerCheckTest("Single if statement true", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", sum);

		auto compilationResult = state.compile(R"(
			sum := 3;

			if (sum == 3) {
				sum := 5;
			}

			return 0;
		)").call<int64_t>();

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}*/

		return sum == 5;
		});

	registerCheckTest("Empty if statement false", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", sum);

		auto compilationResult = state.compile(R"(
			sum := 3;

			if (sum == 3) {} else {
				sum := 5;
			}

			return 0;
		)").call<int64_t>();

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}*/

		return sum == 3;
		});

	registerCheckTest("Empty if statement true", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", sum);

		auto compilationResult = state.compile(R"(
			sum := 4;

			if (sum == 3) {} else {
				sum := 5;
			}

			return 0;
		)").call<int64_t>();

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}*/

		return sum == 5;
		});

	registerCheckTest("Multiple if statement layers", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", sum);

		auto compilationResult = state.compile(R"(
			sum := 0;
			var first := 3;
			var second := 4;

			if (first == 3) 
				if (second == 4)
					sum := 4;

			return 0;
		)").call<int64_t>();

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}*/

		return sum == 4;
		});

	/* TODO
	registerCheckTest("Simple while statement", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", &sum);

		auto compilationResult = state.compile(R"(
			sum = 0;

			while (sum != 3) {
				sum = sum + 1;
			}
		)");

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}//

		return sum == 3;
		});

	/* TODO
	registerCheckTest("Simple while statement (no loop)", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", &sum);
		int64_t test = 0;
		state.defineVariable("test", &test);

		auto compilationResult = state.compile(R"(
			sum = 3;
			test = 0;

			while (sum != 3) {
				sum = sum + 1;
				test = 1;
			}
		)");

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}//

		return sum == 3 && test == 0;
		});

	/* TODO
	registerCheckTest("While else statement (else missed)", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", &sum);
		int64_t test = 0;
		state.defineVariable("test", &test);

		auto compilationResult = state.compile(R"(
			sum = 0;
			test = 0;

			while (sum != 3) {
				sum = sum + 1;
				test = 1;
			} else {
				sum = 7;
				test = 2;
			}
		)");

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}//

		return sum == 3 && test == 1;
		});

	/* TODO
	registerCheckTest("While else statement (else called)", []() {
		KETL::State state;

		int64_t sum = 0;
		state.defineVariable("sum", &sum);
		int64_t test = 0;
		state.defineVariable("test", &test);

		auto compilationResult = state.compile(R"(
			sum = 3;
			test = 0;

			while (sum != 3) {
				sum = sum + 1;
				test = 1;
			} else {
				sum = 7;
				test = 2;
			}
		)");

		/* TODO
		if (std::holds_alternative<std::string>(compilationResult)) {
			std::cerr << std::get<std::string>(compilationResult) << std::endl;
			return false;
		}//

		return sum == 7 && test == 2;
		});*/

	return false;
	};
	
BEFORE_MAIN_ACTION(registerTests);