/*🍲Ketl🍲*/
#include <iostream>
#include <vector>
#include <string>

#include <cstdlib>

extern "C" {
#include "ketl/ketl.h"
#include "ketl/compiler/syntax_solver.h"
#include "executable_memory.h"
#include "compiler/ir_compiler.h"
#include "ketl/function.h"
#include "ketl/atomic_strings.h"
#include "ketl/int_map.h"
}

// TODO rethink iterators using int map iterator as an example
// TODO rethink getOrCreate in int map

int main(int argc, char** argv) {	
	auto source = R"(
	var test := 7;
	return test;
)";


	for (uint64_t i = 0; i < 10000; ++i) {
		KETLState ketlState;

		ketlInitState(&ketlState);

		int64_t testVariable = 4;
		ketlDefineVariable(&ketlState, "test", ketlState.primitives.i64_t, &testVariable);

		auto root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &ketlState.compiler.bytecodeCompiler.syntaxSolver, &ketlState.compiler.bytecodeCompiler.syntaxNodePool);
		
		KETLIRFunction* irFunction = ketlBuildIR(nullptr, &ketlState.compiler.irBuilder, root);

		// TODO optimization on ir

		KETLFunction* function = ketlCompileIR(&ketlState.irCompiler, irFunction);
		free(irFunction);

		int64_t result = 0;
		ketlCallFunction(function, &result);

		std::cout << result << std::endl;
		std::cout << testVariable << std::endl;

		ketlDeinitState(&ketlState);
	}

	return 0;
}