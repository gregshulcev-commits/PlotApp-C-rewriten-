#include "plotapp/PluginManager.h"

#include "plotapp/TextUtil.h"

#include <dlfcn.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace plotapp {
namespace {

constexpr std::size_t kMaxPluginPoints = 2'000'000;

bool isHexColor(const std::string& value) {
    if (value.size() != 4 && value.size() != 7) return false;
    if (value.front() != '#') return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

double parseNumericCell(const std::string& raw, bool* ok) {
    double value = text::toDouble(raw, ok);
    if (ok && *ok) return value;
    std::string normalized = text::trim(raw);
    if (normalized.find(',') != std::string::npos && normalized.find('.') == std::string::npos) {
        std::replace(normalized.begin(), normalized.end(), ',', '.');
        value = text::toDouble(normalized, ok);
        if (ok && *ok) return value;
    }
    return 0.0;
}

std::optional<std::string> getParam(const std::string& params, const std::string& key) {
    const std::string pattern = key + "=";
    const auto pos = params.find(pattern);
    if (pos == std::string::npos) return std::nullopt;
    const auto start = pos + pattern.size();
    const auto end = params.find(';', start);
    return params.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

int getParamInt(const std::string& params, const std::string& key, int fallback, int minValue) {
    const auto raw = getParam(params, key);
    if (!raw.has_value()) return fallback;
    try {
        return std::max(minValue, std::stoi(*raw));
    } catch (...) {
        return fallback;
    }
}

double getParamDouble(const std::string& params, const std::string& key, double fallback) {
    const auto raw = getParam(params, key);
    if (!raw.has_value()) return fallback;
    bool ok = false;
    const double value = text::toDouble(*raw, &ok);
    return ok ? value : fallback;
}

double parseUniformError(const std::string& params) {
    return std::max(0.0, getParamDouble(params, "uniform", 1.0));
}

bool isCandidatePluginFile(const std::filesystem::path& path) {
    return path.extension() == ".so" && path.filename().string().rfind("plotapp_", 0) == 0;
}

std::string canonicalOrLexical(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    return (ec ? path.lexically_normal() : canonical).string();
}

std::optional<std::size_t> resolveImportedErrorColumn(const Layer& sourceLayer, const std::string& params) {
    if (!sourceLayer.importedHeaders.empty()) {
        if (const auto rawIndex = getParam(params, "column_index"); rawIndex.has_value()) {
            try {
                const int index = std::stoi(*rawIndex);
                if (index >= 0 && static_cast<std::size_t>(index) < sourceLayer.importedHeaders.size()) {
                    return static_cast<std::size_t>(index);
                }
            } catch (...) {
                return std::nullopt;
            }
        }

        if (const auto rawName = getParam(params, "column"); rawName.has_value()) {
            for (std::size_t i = 0; i < sourceLayer.importedHeaders.size(); ++i) {
                if (sourceLayer.importedHeaders[i] == *rawName || text::toLower(sourceLayer.importedHeaders[i]) == text::toLower(*rawName)) {
                    return i;
                }
            }
        }
    }
    return std::nullopt;
}

std::vector<double> buildImportedErrorValues(const Layer& sourceLayer, std::size_t columnIndex) {
    if (sourceLayer.importedRows.empty()) {
        throw std::runtime_error("Selected layer does not keep imported table rows for error-bar column lookup");
    }
    if (columnIndex >= sourceLayer.importedHeaders.size()) {
        throw std::runtime_error("Error-bar column index is out of range");
    }

    std::vector<double> values;
    values.reserve(sourceLayer.points.size());
    for (std::size_t i = 0; i < sourceLayer.points.size(); ++i) {
        std::size_t rowIndex = i;
        if (i < sourceLayer.importedRowIndices.size()) rowIndex = sourceLayer.importedRowIndices[i];
        if (rowIndex >= sourceLayer.importedRows.size()) {
            throw std::runtime_error("Imported source row index is out of range for the selected layer");
        }
        const auto& row = sourceLayer.importedRows[rowIndex];
        if (columnIndex >= row.size()) {
            throw std::runtime_error("Selected error column is missing for one or more imported rows");
        }
        bool ok = false;
        const double errorValue = parseNumericCell(row[columnIndex], &ok);
        if (!ok) {
            throw std::runtime_error("Selected error-bar column contains non-numeric values");
        }
        values.push_back(std::max(0.0, errorValue));
    }
    return values;
}

struct ExtremaPoint {
    Point point;
    PointRole role {PointRole::Normal};
};

std::vector<ExtremaPoint> computeLocalExtrema(const Layer& sourceLayer, const std::string& params) {
    std::vector<ExtremaPoint> found;
    if (sourceLayer.points.size() < 3) return found;

    const std::string mode = getParam(params, "mode").value_or("both");
    const int window = getParamInt(params, "window", 3, 1);
    const double tolerance = getParamDouble(params, "tolerance", 0.0);
    const double mergeDx = getParamDouble(params, "merge_dx", 0.0);

    const auto& pts = sourceLayer.points;
    const std::size_t n = pts.size();
    for (std::size_t i = static_cast<std::size_t>(window); i + static_cast<std::size_t>(window) < n; ++i) {
        bool isMin = true;
        bool isMax = true;
        double neighborAverage = 0.0;
        int neighborCount = 0;
        for (int k = -window; k <= window; ++k) {
            if (k == 0) continue;
            const double neighbor = pts[static_cast<std::size_t>(static_cast<int>(i) + k)].y;
            neighborAverage += neighbor;
            ++neighborCount;
            if (pts[i].y > neighbor + tolerance) isMin = false;
            if (pts[i].y < neighbor - tolerance) isMax = false;
        }
        if (!isMin && !isMax) continue;

        PointRole role = PointRole::Normal;
        if (isMin && !isMax) role = PointRole::Minimum;
        else if (!isMin && isMax) role = PointRole::Maximum;
        else {
            const double avg = neighborCount > 0 ? neighborAverage / static_cast<double>(neighborCount) : pts[i].y;
            role = pts[i].y <= avg ? PointRole::Minimum : PointRole::Maximum;
        }
        if (mode == "min" && role != PointRole::Minimum) continue;
        if (mode == "max" && role != PointRole::Maximum) continue;
        if (mode != "min" && mode != "max" && mode != "both") {
            // Unknown mode: keep default behavior of both.
        }

        if (!found.empty() && std::fabs(found.back().point.x - pts[i].x) <= mergeDx) {
            const bool replaceMin = role == PointRole::Minimum && pts[i].y < found.back().point.y;
            const bool replaceMax = role == PointRole::Maximum && pts[i].y > found.back().point.y;
            const bool replaceMixed = mode == "both" && role != found.back().role;
            if (replaceMin || replaceMax || replaceMixed) {
                found.back() = ExtremaPoint{pts[i], role};
            }
        } else {
            found.push_back(ExtremaPoint{pts[i], role});
        }
    }
    return found;
}

} // namespace

struct PluginManager::LoadedPlugin {
    void* handle {nullptr};
    plotapp_get_metadata_fn metadataFn {nullptr};
    plotapp_run_fn runFn {nullptr};
    plotapp_free_result_fn freeFn {nullptr};
    PluginInfo info;
};

PluginManager::PluginManager() = default;

PluginManager::~PluginManager() {
    unloadAll();
}

void PluginManager::unloadAll() {
    for (auto& plugin : loaded_) {
        if (plugin.handle) dlclose(plugin.handle);
        plugin.handle = nullptr;
    }
    loaded_.clear();
}

void PluginManager::addSearchDirectory(std::string directory) {
    if (!directory.empty()) searchDirectories_.push_back(std::move(directory));
}

void PluginManager::discover() {
    unloadAll();
    infoCache_.clear();

    std::unordered_set<std::string> seenPaths;
    std::unordered_set<std::string> seenDirectories;
    std::unordered_set<std::string> seenPluginIds;

    for (const auto& rawDir : searchDirectories_) {
        const std::filesystem::path dirPath(rawDir);
        if (rawDir.empty() || !std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) continue;

        const auto normalizedDir = canonicalOrLexical(dirPath);
        if (!seenDirectories.insert(normalizedDir).second) continue;

        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            std::error_code ec;
            if (!entry.is_regular_file(ec) || ec) continue;
            if (!isCandidatePluginFile(entry.path())) continue;

            const auto fullPath = canonicalOrLexical(entry.path());
            if (!seenPaths.insert(fullPath).second) continue;

            void* handle = dlopen(fullPath.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (!handle) continue;

            auto metadataFn = reinterpret_cast<plotapp_get_metadata_fn>(dlsym(handle, "plotapp_get_metadata"));
            auto runFn = reinterpret_cast<plotapp_run_fn>(dlsym(handle, "plotapp_run"));
            auto freeFn = reinterpret_cast<plotapp_free_result_fn>(dlsym(handle, "plotapp_free_result"));
            if (!metadataFn || !runFn || !freeFn) {
                dlclose(handle);
                continue;
            }

            const auto meta = metadataFn();
            if (meta.api_version != PLOTAPP_PLUGIN_API_VERSION || !meta.plugin_id || !*meta.plugin_id) {
                dlclose(handle);
                continue;
            }

            LoadedPlugin plugin;
            plugin.handle = handle;
            plugin.metadataFn = metadataFn;
            plugin.runFn = runFn;
            plugin.freeFn = freeFn;
            plugin.info.path = fullPath;
            plugin.info.id = meta.plugin_id;
            plugin.info.name = meta.display_name ? meta.display_name : plugin.info.id;
            plugin.info.description = meta.description ? meta.description : "";
            plugin.info.defaultParams = meta.default_params ? meta.default_params : "";

            if (!seenPluginIds.insert(plugin.info.id).second) {
                dlclose(handle);
                continue;
            }

            infoCache_.push_back(plugin.info);
            loaded_.push_back(std::move(plugin));
        }
    }

    std::sort(infoCache_.begin(), infoCache_.end(), [](const PluginInfo& a, const PluginInfo& b) {
        if (a.name == b.name) return a.id < b.id;
        return a.name < b.name;
    });
}

const std::vector<PluginInfo>& PluginManager::plugins() const {
    return infoCache_;
}

bool PluginManager::hasPlugin(const std::string& pluginId) const {
    for (const auto& plugin : loaded_) {
        if (plugin.info.id == pluginId) return true;
    }
    return false;
}

PluginExecutionResult PluginManager::run(const std::string& pluginId, const Layer& sourceLayer, const std::string& params) const {
    for (const auto& plugin : loaded_) {
        if (plugin.info.id != pluginId) continue;

        std::vector<PlotAppPoint> buffer;
        buffer.reserve(sourceLayer.points.size());
        for (const auto& point : sourceLayer.points) {
            buffer.push_back({point.x, point.y});
        }

        PlotAppPluginRequest request{};
        request.source_layer.layer_id = sourceLayer.id.c_str();
        request.source_layer.layer_name = sourceLayer.name.c_str();
        request.source_layer.points = buffer.data();
        request.source_layer.point_count = buffer.size();
        request.params = params.c_str();

        PlotAppPluginResult result{};
        const int rc = plugin.runFn(&request, &result);
        if (rc != 0) {
            plugin.freeFn(&result);
            throw std::runtime_error("Plugin failed: " + pluginId + " (code " + std::to_string(rc) + ")");
        }
        if (result.point_count > kMaxPluginPoints) {
            plugin.freeFn(&result);
            throw std::runtime_error("Plugin returned too many points: " + pluginId);
        }
        if (result.point_count > 0 && !result.points) {
            plugin.freeFn(&result);
            throw std::runtime_error("Plugin returned an invalid point buffer: " + pluginId);
        }

        PluginExecutionResult execution;
        execution.layer.id = makeLayerId();
        execution.layer.type = LayerType::DerivedSeries;
        execution.layer.visible = true;
        execution.layer.sourceLayerId = sourceLayer.id;
        execution.layer.parentLayerId = sourceLayer.id;
        execution.layer.generatorPluginId = pluginId;
        execution.layer.generatorParams = params;
        execution.layer.name = result.suggested_layer_name ? result.suggested_layer_name : (sourceLayer.name + " / " + plugin.info.name);
        execution.layer.legendText = execution.layer.name;
        execution.warning = result.warning_message ? result.warning_message : "";
        execution.layer.style.color = pluginId == "local_extrema" ? "#2ca02c" : (pluginId == "smooth_curve" ? "#9467bd" : "#d62728");
        execution.layer.style.secondaryColor = pluginId == "local_extrema" ? "#d62728" : execution.layer.style.color;
        execution.layer.style.showMarkers = pluginId == "local_extrema";
        execution.layer.style.connectPoints = pluginId != "local_extrema" && pluginId != "error_bars";
        if (pluginId == "local_extrema" || pluginId == "error_bars") execution.layer.style.lineWidth = 1;
        if (!isHexColor(execution.layer.style.color)) execution.layer.style.color = "#1f77b4";
        if (!isHexColor(execution.layer.style.secondaryColor)) execution.layer.style.secondaryColor = execution.layer.style.color;

        execution.layer.points.reserve(result.point_count);
        for (std::size_t i = 0; i < result.point_count; ++i) {
            execution.layer.points.push_back(Point{result.points[i].x, result.points[i].y});
        }

        if (pluginId == "local_extrema") {
            const auto extrema = computeLocalExtrema(sourceLayer, params);
            if (!extrema.empty()) {
                execution.layer.points.clear();
                execution.layer.pointRoles.clear();
                execution.layer.pointVisibility.clear();
                execution.layer.points.reserve(extrema.size());
                execution.layer.pointRoles.reserve(extrema.size());
                execution.layer.pointVisibility.reserve(extrema.size());
                for (const auto& item : extrema) {
                    execution.layer.points.push_back(item.point);
                    execution.layer.pointRoles.push_back(static_cast<int>(item.role));
                    execution.layer.pointVisibility.push_back(1);
                }
            } else {
                execution.layer.pointRoles.assign(execution.layer.points.size(), static_cast<int>(PointRole::Normal));
                execution.layer.pointVisibility.assign(execution.layer.points.size(), 1);
            }
        }

        if (pluginId == "error_bars") {
            execution.layer.points = sourceLayer.points;
            execution.layer.style.color = "#1f77b4";
            execution.layer.style.secondaryColor = execution.layer.style.color;
            execution.layer.style.showMarkers = false;
            execution.layer.style.connectPoints = false;
            execution.layer.style.lineWidth = 1;
            if (const auto importedColumn = resolveImportedErrorColumn(sourceLayer, params); importedColumn.has_value()) {
                execution.layer.errorValues = buildImportedErrorValues(sourceLayer, *importedColumn);
                execution.warning.clear();
            } else if (!sourceLayer.errorValues.empty() && sourceLayer.errorValues.size() == sourceLayer.points.size()) {
                execution.layer.errorValues = sourceLayer.errorValues;
            } else {
                const double uniform = parseUniformError(params);
                execution.layer.errorValues.assign(execution.layer.points.size(), uniform);
            }
        }

        plugin.freeFn(&result);
        return execution;
    }
    throw std::runtime_error("Plugin not found: " + pluginId);
}

} // namespace plotapp
