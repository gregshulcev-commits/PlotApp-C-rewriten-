#pragma once

#include "plotapp/Project.h"

#include <cstddef>
#include <vector>

namespace plotapp {

struct SampledLayerData {
    std::vector<Point> points;
    std::vector<std::size_t> sourceIndices;
};

SampledLayerData sampleLayerForViewport(const Project& project, const Layer& layer,
                                        double viewXMin, double viewXMax,
                                        double viewYMin, double viewYMax,
                                        int samplesHint);

} // namespace plotapp
