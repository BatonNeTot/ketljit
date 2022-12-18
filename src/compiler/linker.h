﻿/*🍲Ketl🍲*/
#ifndef linker_h
#define linker_h

#include "ketl.h"
#include "analyzer.h"

namespace Ketl {

	class Linker {
	public:

		Linker() : _analyzer() {}

		StandaloneFunction proceedStandalone(Environment& env, const std::string& source);

		PureFunction proceed(Environment& env, const std::string& source);

		PureFunction proceed(Environment& env, const Node* commandsNode);

		PureFunction proceed(Environment& env, const uint8_t* bytecode, uint64_t size);

		class Variable {
		public:
			virtual ~Variable() {}
			virtual bool addInstructions(Environment& env, std::vector<Instruction>& instructions) { return false; }
			virtual void propagateArgument(Argument& argument, Argument::Type& type) {}
			virtual uint64_t stackUsage() const { return 0; }
			std::unique_ptr<const Type> type;
			uint64_t stackOffset = 0;
		};

		PureFunction proceed(Environment& env, std::vector<std::unique_ptr<Variable>>& variables, std::vector<Variable*>& stack, const uint8_t* bytecode, uint64_t size);

	private:

		Analyzer _analyzer;
	};
}

#endif /*linker_h*/