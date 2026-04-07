#pragma once

#include "plotapp/Project.h"

#include <string>

namespace plotapp {

class SvgRenderer {
public:
    static void renderToFile(const Project& project, const std::string& path, int width = 1280, int height = 720);
    static std::string renderToString(const Project& project, int width = 1280, int height = 720);
};

} // namespace plotapp
