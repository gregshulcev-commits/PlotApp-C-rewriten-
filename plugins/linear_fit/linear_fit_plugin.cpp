#include "plotapp/PluginApi.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace {

char* dupString(const std::string& value) {
    char* out = static_cast<char*>(std::malloc(value.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, value.c_str(), value.size() + 1);
    return out;
}

int parseSamples(const char* params) {
    if (!params || std::strlen(params) == 0) return 128;
    std::string text(params);
    std::size_t pos = text.find("samples=");
    if (pos == std::string::npos) return 128;
    return std::max(2, std::stoi(text.substr(pos + 8)));
}

} // namespace

extern "C" PlotAppPluginMetadata plotapp_get_metadata() {
    return PlotAppPluginMetadata{PLOTAPP_PLUGIN_API_VERSION, "linear_fit", "Linear approximation", "Least-squares straight line through the selected source layer.", "samples=128"};
}

extern "C" int plotapp_run(const PlotAppPluginRequest* request, PlotAppPluginResult* result) {
    if (!request || !result || request->source_layer.point_count < 2) return 1;
    const auto* points = request->source_layer.points;
    const std::size_t n = request->source_layer.point_count;

    double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
    double minX = points[0].x;
    double maxX = points[0].x;
    for (std::size_t i = 0; i < n; ++i) {
        sumX += points[i].x;
        sumY += points[i].y;
        sumXX += points[i].x * points[i].x;
        sumXY += points[i].x * points[i].y;
        minX = std::min(minX, points[i].x);
        maxX = std::max(maxX, points[i].x);
    }

    double denom = n * sumXX - sumX * sumX;
    if (std::fabs(denom) < 1e-12) return 2;
    double a = (n * sumXY - sumX * sumY) / denom;
    double b = (sumY - a * sumX) / static_cast<double>(n);

    int samples = parseSamples(request->params);
    result->point_count = static_cast<std::size_t>(samples);
    result->points = static_cast<PlotAppPoint*>(std::malloc(sizeof(PlotAppPoint) * result->point_count));
    if (!result->points) return 3;
    for (int i = 0; i < samples; ++i) {
        double x = minX + (maxX - minX) * (static_cast<double>(i) / static_cast<double>(samples - 1));
        result->points[i] = PlotAppPoint{x, a * x + b};
    }
    std::ostringstream name;
    name << request->source_layer.layer_name << " / linear fit";
    result->suggested_layer_name = dupString(name.str());
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
