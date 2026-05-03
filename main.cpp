#include "Arena.h"
#include <iostream>

int main(int argc, char* argv[])
{
    std::string configFile = "config.txt";

    if (argc >= 2) {
        configFile = argv[1];
    }

    Arena arena;

    if (!arena.loadConfig(configFile)) {
        std::cerr << "Using default arena settings.\n";
    }

    arena.run();

    return 0;
}