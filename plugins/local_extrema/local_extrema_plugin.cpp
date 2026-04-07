#include "plotapp/PluginApi.h"
#include "../common/plugin_helpers.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

extern "C" PlotAppPluginMetadata plotapp_get_metadata() {
    return PlotAppPluginMetadata{PLOTAPP_PLUGIN_API_VERSION, "local_extrema", "Min/max finder", "Find local minima and maxima with configurable neighborhood, tolerance and merge distance.", "mode=both;window=3;tolerance=0.0;merge_dx=0.0"};
}

extern "C" int plotapp_run(const PlotAppPluginRequest* request, PlotAppPluginResult* result) {
    if (!request || !result || request->source_layer.point_count < 3) return 1;
    const std::string mode = get_param(request->params, "mode", "both");
    const int window = get_param_int(request->params, "window", 3, 1);
    const double tolerance = get_param_double(request->params, "tolerance", 0.0);
    const double mergeDx = get_param_double(request->params, "merge_dx", 0.0);

    std::vector<PlotAppPoint> found;
    const auto* pts = request->source_layer.points;
    const std::size_t n = request->source_layer.point_count;
    for (std::size_t i = static_cast<std::size_t>(window); i + static_cast<std::size_t>(window) < n; ++i) {
        bool isMin = true;
        bool isMax = true;
        for (int k = -window; k <= window; ++k) {
            if (k == 0) continue;
            const double neighbor = pts[static_cast<std::size_t>(static_cast<int>(i) + k)].y;
            if (pts[i].y > neighbor + tolerance) isMin = false;
            if (pts[i].y < neighbor - tolerance) isMax = false;
        }
        if ((mode == "min" && isMin) || (mode == "max" && isMax) || (mode == "both" && (isMin || isMax))) {
            if (!found.empty() && std::fabs(found.back().x - pts[i].x) <= mergeDx) {
                if ((isMax && pts[i].y > found.back().y) || (isMin && pts[i].y < found.back().y) || mode == "both") {
                    found.back() = pts[i];
                }
            } else {
                found.push_back(pts[i]);
            }
        }
    }

    result->point_count = found.size();
    result->points = static_cast<PlotAppPoint*>(std::malloc(sizeof(PlotAppPoint) * found.size()));
    if (!result->points && !found.empty()) return 2;
    for (std::size_t i = 0; i < found.size(); ++i) result->points[i] = found[i];
    std::ostringstream name;
    name << request->source_layer.layer_name << " / extrema";
    result->suggested_layer_name = plotapp_dup_string(name.str());
    result->warning_message = plotapp_dup_string("Tip: edit the extrema layer points to remove unneeded markers manually if necessary.");
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
