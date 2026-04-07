#include "plotapp/CommandDispatcher.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    plotapp::ProjectController controller;
    plotapp::CommandDispatcher dispatcher(controller);

    if (argc > 1) {
        std::string line;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) line.push_back(' ');
            line += argv[i];
        }
        if (line == "--help" || line == "-h") line = "help";
        std::cout << dispatcher.execute(line) << '\n';
        return 0;
    }

    std::cout << "PlotApp CLI. Type 'help' or 'exit'.\n";
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") break;
        std::cout << dispatcher.execute(line) << '\n';
    }
    return 0;
}
