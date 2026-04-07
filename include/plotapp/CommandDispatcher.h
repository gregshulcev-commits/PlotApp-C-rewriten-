#pragma once

#include "plotapp/ProjectController.h"

#include <filesystem>
#include <string>
#include <vector>

namespace plotapp {

class CommandDispatcher {
public:
    explicit CommandDispatcher(ProjectController& controller);
    std::string execute(const std::string& commandLine);
    std::string help() const;
    std::vector<std::string> completions(const std::string& prefix) const;
    static std::vector<std::string> builtinCommands();

private:
    std::filesystem::path resolvePath(const std::string& raw) const;
    std::string executeShell(const std::string& commandLine) const;
    static std::string bundledHelpText();

    ProjectController& controller_;
    std::filesystem::path currentDirectory_;
};

} // namespace plotapp
