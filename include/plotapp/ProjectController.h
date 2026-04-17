#pragma once

#include "plotapp/Importer.h"
#include "plotapp/PluginManager.h"
#include "plotapp/Project.h"

#include <optional>
#include <string>
#include <vector>

namespace plotapp {

class ProjectController {
public:
    ProjectController();

    Project& project();
    const Project& project() const;
    PluginManager& pluginManager();
    const PluginManager& pluginManager() const;

    void reset();
    TableData previewFile(const std::string& path) const;
    Layer& importLayer(const std::string& path, std::size_t xColumn, std::size_t yColumn, const std::string& layerName = "", std::optional<std::size_t> errorColumn = std::nullopt);
    Layer& createManualLayer(const std::string& name);
    Layer& createFormulaLayer(const std::string& name, const std::string& expression, double xMin, double xMax, int samples);
    void regenerateFormulaLayer(Layer& layer);
    void addPoint(const std::string& layerId, Point point);
    Layer& applyPlugin(const std::string& pluginId, const std::string& sourceLayerId, const std::string& params,
                       const std::vector<std::size_t>& sourcePointSelection = {});
    std::vector<std::string> recomputeDerivedLayers();

    void saveProject(const std::string& path) const;
    void openProject(const std::string& path);
    void exportSvg(const std::string& path, int width = 1280, int height = 720) const;

private:
    const Importer& resolveImporter(const std::string& path) const;

    Project project_;
    DelimitedTextImporter delimitedImporter_;
    XlsxImporter xlsxImporter_;
    PluginManager pluginManager_;
};

} // namespace plotapp
