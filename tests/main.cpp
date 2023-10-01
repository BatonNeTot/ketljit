/*🍲Ketl🍲*/
#include <iostream>
#include <vector>

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
	i64 test := 11;
	if (1 == 1) {
		i64 a := 5;
		i64 b := 3;
		test := a + b;
	} else {
		test := 15;
	}
	return test;
)";

	KETLState ketlState;

	ketlInitState(&ketlState);

	auto root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &ketlState.compiler.bytecodeCompiler.syntaxSolver, &ketlState.compiler.bytecodeCompiler.syntaxNodePool);

	KETLIRFunction* irFunction = ketlBuildIR(nullptr, &ketlState.compiler.irBuilder, root);

	// TODO optimization on ir

	KETLExecutableMemory exeMemory;
	ketlInitExecutableMemory(&exeMemory);

	KETLFunction* function = ketlCompileIR(&exeMemory, irFunction);

	int64_t result = 0;
	ketlCallFunction(function, &result);

	std::cout << result << std::endl;

	ketlDeinitExecutableMemory(&exeMemory);

	ketlDeinitState(&ketlState);

	return 0;
}