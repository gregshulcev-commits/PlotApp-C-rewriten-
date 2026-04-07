#include "plotapp/PluginApi.h"
#include "../common/plugin_helpers.h"

#include <sstream>

extern "C" PlotAppPluginMetadata plotapp_get_metadata() {
    return PlotAppPluginMetadata{PLOTAPP_PLUGIN_API_VERSION, "error_bars", "Error bars", "Create an error-bar layer from the selected layer. Params: uniform=<total_error> or column=<header>/column_index=<n> from the imported table.", "uniform=1.0"};
}

extern "C" int plotapp_run(const PlotAppPluginRequest* request, PlotAppPluginResult* result) {
    if (!request || !result) return 1;
    const std::size_t n = request->source_layer.point_count;
    result->points = static_cast<PlotAppPoint*>(std::malloc(sizeof(PlotAppPoint) * n));
    if (!result->points && n > 0) return 2;
    result->point_count = n;
    for (std::size_t i = 0; i < n; ++i) result->points[i] = request->source_layer.points[i];
    std::ostringstream name;
    name << request->source_layer.layer_name << " / error bars";
    result->suggested_layer_name = plotapp_dup_string(name.str());
    result->warning_message = plotapp_dup_string("Use an imported table column, existing source errors, or uniform=<value> to define total error height.");
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
