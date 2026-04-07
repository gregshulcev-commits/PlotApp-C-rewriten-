#include "plotapp/PluginApi.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <string>

namespace {
char* dupString(const std::string& value) {
    char* out = static_cast<char*>(std::malloc(value.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, value.c_str(), value.size() + 1);
    return out;
}

int parseWindow(const char* params) {
    if (!params || std::strlen(params) == 0) return 3;
    std::string text(params);
    std::size_t pos = text.find("window=");
    if (pos == std::string::npos) return 3;
    return std::max(2, std::stoi(text.substr(pos + 7)));
}
}

extern "C" PlotAppPluginMetadata plotapp_get_metadata() {
    return PlotAppPluginMetadata{PLOTAPP_PLUGIN_API_VERSION, "moving_average", "Moving average", "Centered moving average smoothing for noisy source layers.", "window=3"};
}

extern "C" int plotapp_run(const PlotAppPluginRequest* request, PlotAppPluginResult* result) {
    if (!request || !result || request->source_layer.point_count < 2) return 1;
    const auto* points = request->source_layer.points;
    std::size_t n = request->source_layer.point_count;
    int window = parseWindow(request->params);
    result->point_count = n;
    result->points = static_cast<PlotAppPoint*>(std::malloc(sizeof(PlotAppPoint) * n));
    if (!result->points) return 2;
    int radius = window / 2;
    for (std::size_t i = 0; i < n; ++i) {
        int from = std::max<int>(0, static_cast<int>(i) - radius);
        int to = std::min<int>(static_cast<int>(n) - 1, static_cast<int>(i) + radius);
        double sum = 0.0;
        int count = 0;
        for (int j = from; j <= to; ++j) {
            sum += points[j].y;
            ++count;
        }
        result->points[i] = PlotAppPoint{points[i].x, sum / static_cast<double>(count)};
    }
    std::ostringstream name;
    name << request->source_layer.layer_name << " / moving average";
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
