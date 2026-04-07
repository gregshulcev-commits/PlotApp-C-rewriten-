#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLOTAPP_PLUGIN_API_VERSION 1

typedef struct PlotAppPoint {
    double x;
    double y;
} PlotAppPoint;

typedef struct PlotAppLayerView {
    const char* layer_id;
    const char* layer_name;
    const PlotAppPoint* points;
    size_t point_count;
} PlotAppLayerView;

typedef struct PlotAppPluginMetadata {
    int api_version;
    const char* plugin_id;
    const char* display_name;
    const char* description;
    const char* default_params;
} PlotAppPluginMetadata;

typedef struct PlotAppPluginRequest {
    PlotAppLayerView source_layer;
    const char* params;
} PlotAppPluginRequest;

typedef struct PlotAppPluginResult {
    PlotAppPoint* points;
    size_t point_count;
    char* suggested_layer_name;
    char* warning_message;
} PlotAppPluginResult;

typedef PlotAppPluginMetadata (*plotapp_get_metadata_fn)();
typedef int (*plotapp_run_fn)(const PlotAppPluginRequest* request, PlotAppPluginResult* result);
typedef void (*plotapp_free_result_fn)(PlotAppPluginResult* result);

#ifdef __cplusplus
}
#endif
