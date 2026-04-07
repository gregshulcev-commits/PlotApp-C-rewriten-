#pragma once

#include "plotapp/Project.h"

#include <string>

namespace plotapp {

class ProjectSerializer {
public:
    static void save(const Project& project, const std::string& path);
    static Project load(const std::string& path);
};

} // namespace plotapp
