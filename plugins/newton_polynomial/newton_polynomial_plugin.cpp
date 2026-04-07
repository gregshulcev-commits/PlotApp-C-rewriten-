#include "plotapp/CurveMath.h"
#include "plotapp/PluginApi.h"
#include "../common/plugin_helpers.h"

#include <algorithm>
#include <sstream>
#include <vector>

extern "C" PlotAppPluginMetadata plotapp_get_metadata() {
    return PlotAppPluginMetadata{PLOTAPP_PLUGIN_API_VERSION, "newton_polynomial", "Newton polynomial", "Polynomial approximation with configurable degree and sampled output.", "degree=2;samples=256"};
}

extern "C" int plotapp_run(const PlotAppPluginRequest* request, PlotAppPluginResult* result) {
    if (!request || !result || request->source_layer.point_count < 2) return 1;

    std::vector<plotapp::Point> points;
    points.reserve(request->source_layer.point_count);
    for (std::size_t i = 0; i < request->source_layer.point_count; ++i) {
        points.push_back(plotapp::Point{request->source_layer.points[i].x, request->source_layer.points[i].y});
    }

    const int degree = std::clamp(get_param_int(request->params, "degree", 2, 1), 1, 12);
    std::vector<double> coeffs;
    if (!plotapp::math::fitPolynomialRegression(points, degree, coeffs)) return 2;
    const auto xRange = plotapp::math::finiteXRange(points);
    const int samples = get_param_int(request->params, "samples", 256, 2);
    const auto polyPoints = plotapp::math::samplePolynomial(coeffs, xRange.first, xRange.second, samples);
    if (polyPoints.size() < 2) return 3;

    result->point_count = polyPoints.size();
    result->points = static_cast<PlotAppPoint*>(std::malloc(sizeof(PlotAppPoint) * result->point_count));
    if (!result->points) return 4;
    for (std::size_t i = 0; i < result->point_count; ++i) {
        result->points[i] = PlotAppPoint{polyPoints[i].x, polyPoints[i].y};
    }

    std::ostringstream name;
    name << request->source_layer.layer_name << " / newton poly deg" << degree;
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
