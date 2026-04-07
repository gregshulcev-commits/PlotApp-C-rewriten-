#include "plotapp/CurveMath.h"
#include "plotapp/PluginApi.h"
#include "../common/plugin_helpers.h"

#include <sstream>
#include <vector>

extern "C" PlotAppPluginMetadata plotapp_get_metadata() {
    return PlotAppPluginMetadata{PLOTAPP_PLUGIN_API_VERSION, "smooth_curve", "Smooth curve", "Connect all source points with a smooth natural cubic spline.", "samples=256"};
}

extern "C" int plotapp_run(const PlotAppPluginRequest* request, PlotAppPluginResult* result) {
    if (!request || !result || request->source_layer.point_count < 2) return 1;

    std::vector<plotapp::Point> points;
    points.reserve(request->source_layer.point_count);
    for (std::size_t i = 0; i < request->source_layer.point_count; ++i) {
        points.push_back(plotapp::Point{request->source_layer.points[i].x, request->source_layer.points[i].y});
    }

    const auto xRange = plotapp::math::finiteXRange(points);
    const int samples = get_param_int(request->params, "samples", 256, 2);
    const auto splinePoints = plotapp::math::sampleSpline(points, xRange.first, xRange.second, samples, true);
    if (splinePoints.size() < 2) return 2;

    result->point_count = splinePoints.size();
    result->points = static_cast<PlotAppPoint*>(std::malloc(sizeof(PlotAppPoint) * result->point_count));
    if (!result->points) return 3;
    for (std::size_t i = 0; i < result->point_count; ++i) {
        result->points[i] = PlotAppPoint{splinePoints[i].x, splinePoints[i].y};
    }

    std::ostringstream name;
    name << request->source_layer.layer_name << " / smooth curve";
    result->suggested_layer_name = plotapp_dup_string(name.str());
    result->warning_message = nullptr;
    return 0;
}

extern "C" void plotapp_free_result(PlotAppPluginResult* result) {
    if (!result) return;
    std::free(result->points);
    std::free(result->suggested_layer_name);
    std::free(result->warning_message);
    result->points = nullptr;
    result->suggested_layer_name = nullptr;
    result->warning_message = nullptr;
    result->point_count = 0;
}
