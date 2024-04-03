//🫖ketl
#ifndef ketl_speed_tests_h
#define ketl_speed_tests_h

#include "compile_tricks.h"

#include "ketl/ketl.hpp"

#include "lua.hpp"

#include <chrono>
#include <functional>

using SpeedTestFunction = std::function<void(uint64_t, double&, double&)>;

void registerSpeedTest(std::string_view&& name, SpeedTestFunction&& test);

void launchSpeedTests(uint64_t N);

#endif // ketl_speed_tests_h