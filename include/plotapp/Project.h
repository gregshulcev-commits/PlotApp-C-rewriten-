#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace plotapp {

struct Point {
    double x {0.0};
    double y {0.0};
};

struct LayerStyle {
    std::string color {"#1f77b4"};
    std::string secondaryColor {"#d62728"};
    int lineWidth {2};
    bool showMarkers {true};
    bool connectPoints {true};
};

enum class LayerType {
    RawSeries,
    DerivedSeries,
    FormulaSeries
};

enum class PointRole {
    Normal = 0,
    Minimum = 1,
    Maximum = 2
};

struct Layer {
    std::string id;
    std::string name;
    LayerType type {LayerType::RawSeries};
    bool visible {true};
    LayerStyle style {};
    std::vector<Point> points;
    std::vector<double> errorValues;
    std::vector<int> pointRoles;
    std::vector<int> pointVisibility;
    std::string sourceLayerId;
    std::string parentLayerId;
    std::string generatorPluginId;
    std::string generatorParams;
    std::vector<std::size_t> pluginSourcePointIndices;
    std::string notes;

    std::string formulaExpression;
    double formulaXMin {-10.0};
    double formulaXMax {10.0};
    int formulaSamples {512};

    std::string importedSourcePath;
    std::string importedSheetName;
    std::vector<std::string> importedHeaders;
    std::vector<std::vector<std::string>> importedRows;
    std::vector<std::size_t> importedRowIndices;
    std::size_t importedXColumn {0};
    std::size_t importedYColumn {1};

    bool legendVisible {true};
    std::string legendText;
    double legendAnchorX {0.82};
    double legendAnchorY {0.08};
};

struct ProjectSettings {
    std::string title {"PlotApp Project"};
    std::string xLabel {"X"};
    std::string yLabel {"Y"};
    bool showGrid {true};
    bool hasCustomViewport {false};
    double viewXMin {-10.0};
    double viewXMax {10.0};
    double viewYMin {-10.0};
    double viewYMax {10.0};
    std::string uiTheme {"dark"};
    int uiFontPercent {100};
};

class Project {
public:
    Project();

    const ProjectSettings& settings() const;
    ProjectSettings& settings();
    const std::vector<Layer>& layers() const;
    std::vector<Layer>& layers();

    Layer& createLayer(std::string name, LayerType type = LayerType::RawSeries);
    bool removeLayer(const std::string& layerId);
    Layer* findLayer(const std::string& layerId);
    const Layer* findLayer(const std::string& layerId) const;

    std::vector<const Layer*> visibleLayers() const;
    std::vector<Layer*> mutableVisibleLayers();

private:
    ProjectSettings settings_;
    std::vector<Layer> layers_;
    int nextLayerNumber_ {1};
};

std::string layerTypeToString(LayerType type);
LayerType layerTypeFromString(const std::string& value);
std::string makeLayerId();

inline bool pointIsVisible(const Layer& layer, std::size_t index) {
    return index >= layer.pointVisibility.size() || layer.pointVisibility[index] != 0;
}

inline bool layerSupportsPointRoles(const Layer& layer) {
    return layer.generatorPluginId == "local_extrema";
}

inline PointRole pointRoleAt(const Layer& layer, std::size_t index) {
    if (!layerSupportsPointRoles(layer)) return PointRole::Normal;
    if (index >= layer.pointRoles.size()) return PointRole::Normal;
    const int raw = layer.pointRoles[index];
    if (raw == static_cast<int>(PointRole::Minimum)) return PointRole::Minimum;
    if (raw == static_cast<int>(PointRole::Maximum)) return PointRole::Maximum;
    return PointRole::Normal;
}

} // namespace plotapp
