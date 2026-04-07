#pragma once

#include "plotapp/Project.h"

#include <string>
#include <vector>

namespace plotapp {

class FormulaEvaluator {
public:
    static void validate(const std::string& expression);
    static double evaluate(const std::string& expression, double x);
    static std::vector<Point> sample(const std::string& expression, double xMin, double xMax, int samples);
};

} // namespace plotapp
