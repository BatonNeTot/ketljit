#include "ketl/ketl.hpp"

#include <iostream>
#include <vector>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    KETL::State ketlState;

	std::vector<KETLParameter> parameters = {};
    std::string line;

    while (std::cin) {
        std::cout << ">> " << std::flush;
        std::getline(std::cin, line);
        KETL::Function function = ketlState.compile(line, parameters.data(), parameters.size());
        if (!function) {
            continue;
        }

	    function.callVoid();
    }

    return 0;
}