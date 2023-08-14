﻿/*🍲Ketl🍲*/
#include <iostream>

extern "C" {
#include "ketl/ketl.h"
#include "ketl/compiler/syntax_solver.h"
#include "compiler/ir_node.h"
#include "compiler/ir_compiler.h"
#include "ketl/function.h"
#include "ketl/atomic_strings.h"
#include "ketl/int_map.h"
}

// TODO rethink iterators using int map iterator as an example
// TODO rethink getOrCreate in int map

int main(int argc, char** argv) {	
	auto source = R"(
	return 7;
)";

	KETLState ketlState;

	ketlInitState(&ketlState);

	auto root = ketlSolveSyntax(source, KETL_NULL_TERMINATED_LENGTH, &ketlState.compiler.bytecodeCompiler.syntaxSolver, &ketlState.compiler.bytecodeCompiler.syntaxNodePool);

	KETLIRState irState;

	ketlBuildIR(nullptr, &ketlState.compiler.irBuilder, &irState, root);

	// TODO optimization on ir

	KETLFunction* function = ketlCompileIR(&irState);

	int64_t result = 0;
	uint8_t* stack = new uint8_t[function->stackSize + sizeof(uint64_t)];
	ketlCallFunction(function, stack, &result);

	std::cout << result << std::endl;

	ketlDeinitState(&ketlState);

	return 0;
}