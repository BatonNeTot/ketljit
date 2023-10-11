﻿/*🍲Ketl🍲*/
#ifndef check_tests_h
#define check_tests_h

#include "compile_tricks.h"

#include "ketl/ketl.hpp"

#include <string_view>
#include <functional>

using CheckTestFunction = std::function<bool()>;

void registerCheckTest(std::string_view&& name, CheckTestFunction&& test);

void launchCheckTests();

#endif // check_tests_h