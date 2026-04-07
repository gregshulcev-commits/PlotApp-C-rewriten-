#include "plotapp/PluginApi.h"
#include "../common/plugin_helpers.h"
extern "C" PlotAppPluginMetadata plotapp_get_metadata() {
    return PlotAppPluginMetadata{PLOTAPP_PLUGIN_API_VERSION, "newton_deg4", "Newton polynomial deg4", "Fourth-degree approximation in polynomial/Newton basis for the selected layer.", "samples=256"};
}
extern "C" int plotapp_run(const PlotAppPluginRequest* request, PlotAppPluginResult* result) { return run_polynomial_fit(request, result, 4, "newton deg4"); }
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
