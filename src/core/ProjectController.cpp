#include "plotapp/ProjectController.h"

#include "plotapp/FormulaEvaluator.h"
#include "plotapp/ProjectSerializer.h"
#include "plotapp/SvgRenderer.h"
#include "plotapp/TextUtil.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace plotapp {
namespace {

std::string columnLabelFor(const TableData& table, std::size_t index) {
    if (index < table.headers.size() && !text::trim(table.headers[index]).empty()) {
        return text::trim(table.headers[index]);
    }
    return "Column " + std::to_string(index + 1);
}

std::string importedLayerDisplayName(const TableData& table, std::size_t xColumn, std::size_t yColumn) {
    const auto xLabel = columnLabelFor(table, xColumn);
    const auto yLabel = columnLabelFor(table, yColumn);
    if (!xLabel.empty() && !yLabel.empty() && xLabel != yLabel) return yLabel + " vs " + xLabel;
    if (!yLabel.empty()) return yLabel;
    if (!xLabel.empty()) return xLabel;
    const std::filesystem::path sourcePath(table.sourcePath);
    return sourcePath.stem().empty() ? "Imported layer" : sourcePath.stem().string();
}

std::string defaultFormulaLayerName(const std::string& normalizedExpression) {
    return normalizedExpression.empty() ? std::string("Formula") : normalizedExpression;
}

void normalizeFormulaLayerInputs(std::string& expression, double& xMin, double& xMax, int& samples) {
    expression = text::trim(expression);
    FormulaEvaluator::validate(expression);
    if (!std::isfinite(xMin) || !std::isfinite(xMax)) {
        throw std::runtime_error("Formula layer range must be finite");
    }
    if (xMin == xMax) {
        throw std::runtime_error("Formula layer range cannot be zero-width");
    }
    if (xMin > xMax) std::swap(xMin, xMax);
    if (samples < 2) samples = 2;
}

std::vector<std::string> splitSearchPathList(const char* raw) {
    std::vector<std::string> out;
    if (!raw || !*raw) return out;

    std::string current;
    for (const char c : std::string(raw)) {
        if (c == ':') {
            if (!current.empty()) out.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty()) out.push_back(current);
    return out;
}

std::filesystem::path executableDirectory() {
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) return {};
    auto normalized = std::filesystem::weakly_canonical(exe, ec);
    if (!ec) exe = normalized;
    return exe.parent_path();
}

std::vector<std::string> defaultPluginSearchDirectories() {
    std::vector<std::string> out;
    for (const auto& fromEnv : splitSearchPathList(std::getenv("PLOTAPP_PLUGIN_DIR"))) {
        out.push_back(fromEnv);
    }

    const auto exeDir = executableDirectory();
    if (!exeDir.empty()) {
        out.push_back((exeDir / "plugins").string());
        out.push_back((exeDir.parent_path() / "plugins").string());
        out.push_back((exeDir.parent_path() / "lib" / "plotapp" / "plugins").string());
        out.push_back((exeDir.parent_path() / "lib64" / "plotapp" / "plugins").string());
    }
    return out;
}

bool nearlyEqual(double a, double b) {
    const double scale = std::max({1.0, std::fabs(a), std::fabs(b)});
    return std::fabs(a - b) <= scale * 1e-9;
}

std::vector<std::size_t> normalizeSourcePointSelection(const Layer& sourceLayer, const std::vector<std::size_t>& requested) {
    std::vector<std::size_t> normalized;
    if (sourceLayer.points.empty()) return normalized;

    if (requested.empty()) {
        normalized.reserve(sourceLayer.points.size());
        for (std::size_t i = 0; i < sourceLayer.points.size(); ++i) normalized.push_back(i);
        return normalized;
    }

    std::vector<int> selected(sourceLayer.points.size(), 0);
    for (const auto index : requested) {
        if (index < sourceLayer.points.size()) selected[index] = 1;
    }

    normalized.reserve(sourceLayer.points.size());
    for (std::size_t i = 0; i < sourceLayer.points.size(); ++i) {
        if (selected[i] != 0) normalized.push_back(i);
    }
    return normalized;
}

bool selectionCoversWholeLayer(const Layer& sourceLayer, const std::vector<std::size_t>& normalizedSelection) {
    return !sourceLayer.points.empty() && normalizedSelection.size() == sourceLayer.points.size();
}

Layer buildSelectedSourceLayer(const Layer& sourceLayer, const std::vector<std::size_t>& normalizedSelection) {
    if (selectionCoversWholeLayer(sourceLayer, normalizedSelection)) return sourceLayer;

    Layer selected = sourceLayer;
    selected.points.clear();
    selected.errorValues.clear();
    selected.pointRoles.clear();
    selected.pointVisibility.clear();
    selected.importedRowIndices.clear();

    selected.points.reserve(normalizedSelection.size());
    if (!sourceLayer.errorValues.empty()) selected.errorValues.reserve(normalizedSelection.size());
    if (!sourceLayer.pointRoles.empty()) selected.pointRoles.reserve(normalizedSelection.size());
    if (!sourceLayer.pointVisibility.empty()) selected.pointVisibility.reserve(normalizedSelection.size());
    if (!sourceLayer.importedRowIndices.empty()) selected.importedRowIndices.reserve(normalizedSelection.size());

    for (const auto index : normalizedSelection) {
        if (index >= sourceLayer.points.size()) continue;
        selected.points.push_back(sourceLayer.points[index]);
        if (index < sourceLayer.errorValues.size()) selected.errorValues.push_back(sourceLayer.errorValues[index]);
        if (index < sourceLayer.pointRoles.size()) selected.pointRoles.push_back(sourceLayer.pointRoles[index]);
        if (index < sourceLayer.pointVisibility.size()) selected.pointVisibility.push_back(sourceLayer.pointVisibility[index]);
        if (index < sourceLayer.importedRowIndices.size()) selected.importedRowIndices.push_back(sourceLayer.importedRowIndices[index]);
    }
    return selected;
}

std::vector<int> mergePointVisibility(const Layer& existingLayer, const Layer& recomputedLayer) {
    std::vector<int> merged(recomputedLayer.points.size(), 1);
    if (existingLayer.pointVisibility.empty() || existingLayer.points.empty() || recomputedLayer.points.empty()) {
        return merged;
    }

    std::vector<int> used(existingLayer.points.size(), 0);
    for (std::size_t i = 0; i < recomputedLayer.points.size(); ++i) {
        const auto newRole = pointRoleAt(recomputedLayer, i);
        for (std::size_t j = 0; j < existingLayer.points.size(); ++j) {
            if (used[j] != 0) continue;
            if (pointRoleAt(existingLayer, j) != newRole) continue;
            if (!nearlyEqual(existingLayer.points[j].x, recomputedLayer.points[i].x)) continue;
            if (!nearlyEqual(existingLayer.points[j].y, recomputedLayer.points[i].y)) continue;
            merged[i] = pointIsVisible(existingLayer, j) ? 1 : 0;
            used[j] = 1;
            break;
        }
    }
    return merged;
}

LayerStyle mergeStyleAfterRecompute(const Layer& existingLayer, const Layer& recomputedLayer) {
    LayerStyle style = recomputedLayer.style;

    if (!existingLayer.style.color.empty()) style.color = existingLayer.style.color;
    if (!existingLayer.style.secondaryColor.empty()) style.secondaryColor = existingLayer.style.secondaryColor;
    if (existingLayer.style.lineWidth > 0 && existingLayer.style.lineWidth != recomputedLayer.style.lineWidth) {
        style.lineWidth = existingLayer.style.lineWidth;
    }

    if (existingLayer.generatorPluginId == "local_extrema") {
        style.showMarkers = true;
        style.connectPoints = false;
    } else if (existingLayer.generatorPluginId == "error_bars") {
        style.showMarkers = false;
        style.connectPoints = false;
    } else {
        style.showMarkers = existingLayer.style.showMarkers;
        style.connectPoints = existingLayer.style.connectPoints;
    }

    return style;
}

} // namespace

ProjectController::ProjectController() {
    for (const auto& directory : defaultPluginSearchDirectories()) {
        pluginManager_.addSearchDirectory(directory);
    }
    pluginManager_.discover();
}

Project& ProjectController::project() { return project_; }
const Project& ProjectController::project() const { return project_; }
PluginManager& ProjectController::pluginManager() { return pluginManager_; }
const PluginManager& ProjectController::pluginManager() const { return pluginManager_; }

void ProjectController::reset() {
    project_ = Project{};
}

const Importer& ProjectController::resolveImporter(const std::string& path) const {
    if (xlsxImporter_.supports(path)) return xlsxImporter_;
    if (delimitedImporter_.supports(path)) return delimitedImporter_;
    throw std::runtime_error("Unsupported file type: " + path);
}

TableData ProjectController::previewFile(const std::string& path) const {
    return resolveImporter(path).load(path);
}

Layer& ProjectController::importLayer(const std::string& path, std::size_t xColumn, std::size_t yColumn, const std::string& layerName, std::optional<std::size_t> errorColumn) {
    auto table = previewFile(path);
    auto numeric = extractNumericSeries(table, xColumn, yColumn, errorColumn);
    const std::string displayName = text::trim(layerName).empty() ? importedLayerDisplayName(table, xColumn, yColumn) : layerName;
    auto& layer = project_.createLayer(displayName, LayerType::RawSeries);
    layer.points = std::move(numeric.points);
    layer.errorValues = std::move(numeric.errorValues);
    layer.importedSourcePath = table.sourcePath;
    layer.importedSheetName = table.sheetName;
    layer.importedHeaders = table.headers;
    layer.importedRows = table.rows;
    layer.importedRowIndices = std::move(numeric.rowIndices);
    layer.importedXColumn = xColumn;
    layer.importedYColumn = yColumn;
    layer.legendText = layer.name;
    project_.settings().xLabel = numeric.xLabel;
    project_.settings().yLabel = numeric.yLabel;
    project_.settings().hasCustomViewport = false;
    return layer;
}

Layer& ProjectController::createManualLayer(const std::string& name) {
    return project_.createLayer(name, LayerType::RawSeries);
}

Layer& ProjectController::createFormulaLayer(const std::string& name, const std::string& expression, double xMin, double xMax, int samples) {
    std::string normalizedExpression = expression;
    normalizeFormulaLayerInputs(normalizedExpression, xMin, xMax, samples);
    auto points = FormulaEvaluator::sample(normalizedExpression, xMin, xMax, samples);
    const std::string displayName = text::trim(name).empty() ? defaultFormulaLayerName(normalizedExpression) : name;
    auto& layer = project_.createLayer(displayName, LayerType::FormulaSeries);
    layer.formulaExpression = std::move(normalizedExpression);
    layer.formulaXMin = xMin;
    layer.formulaXMax = xMax;
    layer.formulaSamples = samples;
    layer.style.showMarkers = false;
    layer.style.connectPoints = true;
    layer.points = std::move(points);
    layer.legendText = layer.formulaExpression.empty() ? layer.name : layer.formulaExpression;
    return layer;
}

void ProjectController::regenerateFormulaLayer(Layer& layer) {
    if (layer.type != LayerType::FormulaSeries) {
        throw std::runtime_error("Layer is not formula-based");
    }
    normalizeFormulaLayerInputs(layer.formulaExpression, layer.formulaXMin, layer.formulaXMax, layer.formulaSamples);
    layer.points = FormulaEvaluator::sample(layer.formulaExpression, layer.formulaXMin, layer.formulaXMax, layer.formulaSamples);
    if (text::trim(layer.name).empty()) layer.name = defaultFormulaLayerName(layer.formulaExpression);
    if (layer.legendText.empty()) layer.legendText = layer.formulaExpression.empty() ? layer.name : layer.formulaExpression;
}

void ProjectController::addPoint(const std::string& layerId, Point point) {
    auto* layer = project_.findLayer(layerId);
    if (!layer) throw std::runtime_error("Layer not found: " + layerId);
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::runtime_error("Point coordinates must be finite");
    }
    layer->points.push_back(point);
    if (layer->type == LayerType::FormulaSeries) {
        layer->type = LayerType::RawSeries;
        layer->formulaExpression.clear();
    }
}

Layer& ProjectController::applyPlugin(const std::string& pluginId, const std::string& sourceLayerId, const std::string& params,
                                    const std::vector<std::size_t>& sourcePointSelection) {
    auto* source = project_.findLayer(sourceLayerId);
    if (!source) throw std::runtime_error("Source layer not found: " + sourceLayerId);

    const auto normalizedSelection = normalizeSourcePointSelection(*source, sourcePointSelection);
    if (normalizedSelection.empty()) {
        throw std::runtime_error("No source points selected for plugin application");
    }

    auto result = pluginManager_.run(pluginId, buildSelectedSourceLayer(*source, normalizedSelection), params);
    if (result.layer.legendText.empty()) result.layer.legendText = result.layer.name;
    result.layer.parentLayerId = source->id;
    result.layer.pluginSourcePointIndices = selectionCoversWholeLayer(*source, normalizedSelection)
        ? std::vector<std::size_t>{}
        : normalizedSelection;
    project_.layers().push_back(std::move(result.layer));
    return project_.layers().back();
}

std::vector<std::string> ProjectController::recomputeDerivedLayers() {
    std::vector<std::string> warnings;
    for (auto& layer : project_.layers()) {
        if (layer.type == LayerType::FormulaSeries) {
            try {
                regenerateFormulaLayer(layer);
            } catch (const std::exception& ex) {
                warnings.push_back("Formula layer recompute failed for " + layer.name + ": " + ex.what());
            }
            continue;
        }
        if (layer.type != LayerType::DerivedSeries || layer.generatorPluginId.empty() || layer.sourceLayerId.empty()) continue;
        auto* source = project_.findLayer(layer.sourceLayerId);
        if (!source) {
            warnings.push_back("Source layer not found for derived layer: " + layer.name);
            continue;
        }
        if (!pluginManager_.hasPlugin(layer.generatorPluginId)) {
            warnings.push_back("Plugin not available, using stored points for layer: " + layer.name);
            continue;
        }
        try {
            const auto normalizedSelection = normalizeSourcePointSelection(*source, layer.pluginSourcePointIndices);
            if (!layer.pluginSourcePointIndices.empty() && normalizedSelection.empty()) {
                warnings.push_back("Stored point selection is no longer valid for layer: " + layer.name);
                continue;
            }

            auto result = pluginManager_.run(layer.generatorPluginId, buildSelectedSourceLayer(*source, normalizedSelection), layer.generatorParams);
            const Layer previousLayer = layer;
            layer.pluginSourcePointIndices = selectionCoversWholeLayer(*source, normalizedSelection)
                ? std::vector<std::size_t>{}
                : normalizedSelection;
            layer.points = std::move(result.layer.points);
            layer.errorValues = std::move(result.layer.errorValues);
            layer.pointRoles = std::move(result.layer.pointRoles);
            if (!layer.pointRoles.empty() || !previousLayer.pointVisibility.empty()) {
                layer.pointVisibility = mergePointVisibility(previousLayer, layer);
            } else {
                layer.pointVisibility = std::move(result.layer.pointVisibility);
            }
            layer.style = mergeStyleAfterRecompute(previousLayer, result.layer);
            if (!result.warning.empty()) warnings.push_back(result.warning);
        } catch (const std::exception& ex) {
            warnings.push_back("Plugin recompute failed for " + layer.name + ": " + ex.what());
        }
    }
    return warnings;
}

void ProjectController::saveProject(const std::string& path) const {
    ProjectSerializer::save(project_, path);
}

void ProjectController::openProject(const std::string& path) {
    project_ = ProjectSerializer::load(path);
}

void ProjectController::exportSvg(const std::string& path, int width, int height) const {
    SvgRenderer::renderToFile(project_, path, width, height);
}

} // namespace plotapp
