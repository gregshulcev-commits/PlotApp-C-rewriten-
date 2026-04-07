#include "plotapp/ProjectSerializer.h"
#include "plotapp/TextUtil.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace plotapp {
namespace {

constexpr int kCurrentProjectFormatVersion = 5;
constexpr char kJoinedFieldSeparator = '\x1F';

void writeKV(std::ostream& out, const std::string& key, const std::string& value) {
    out << key << '=' << text::escape(value) << '\n';
}

std::string tempSavePathFor(const std::string& path) {
    return path + ".tmp";
}

int parseIntField(const std::string& field, const std::string& value, std::size_t lineNumber) {
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid integer for " + field + " at line " + std::to_string(lineNumber));
    }
}

double parseDoubleField(const std::string& field, const std::string& value, std::size_t lineNumber) {
    try {
        const double parsed = std::stod(value);
        if (!std::isfinite(parsed)) {
            throw std::runtime_error("Non-finite numeric value");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid numeric value for " + field + " at line " + std::to_string(lineNumber));
    }
}

std::string joinFields(const std::vector<std::string>& items) {
    std::string joined;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) joined.push_back(kJoinedFieldSeparator);
        joined += items[i];
    }
    return joined;
}

std::vector<std::string> splitFields(const std::string& value) {
    if (value.empty()) return {};
    return text::split(value, kJoinedFieldSeparator);
}

void normalizePointMetadata(Layer& layer) {
    if (layer.pointRoles.size() > layer.points.size()) layer.pointRoles.resize(layer.points.size());
    if (layer.pointRoles.size() < layer.points.size()) {
        layer.pointRoles.resize(layer.points.size(), static_cast<int>(PointRole::Normal));
    }
    if (layer.pointVisibility.size() > layer.points.size()) layer.pointVisibility.resize(layer.points.size());
    if (!layer.pointVisibility.empty() && layer.pointVisibility.size() < layer.points.size()) {
        layer.pointVisibility.resize(layer.points.size(), 1);
    }
    if (layer.importedRowIndices.size() > layer.points.size()) layer.importedRowIndices.resize(layer.points.size());
    if (layer.errorValues.size() > layer.points.size()) layer.errorValues.resize(layer.points.size());
}

void validateLoadedProject(Project& project) {
    for (auto& layer : project.layers()) {
        if (layer.id.empty()) layer.id = makeLayerId();
        if (layer.name.empty()) layer.name = "Layer";
        if (layer.legendText.empty()) layer.legendText = layer.name;
        if (layer.formulaSamples < 2) layer.formulaSamples = 2;
        if (layer.style.lineWidth < 1) layer.style.lineWidth = 1;
        if (layer.legendAnchorX < 0.0) layer.legendAnchorX = 0.0;
        if (layer.legendAnchorX > 1.0) layer.legendAnchorX = 1.0;
        if (layer.legendAnchorY < 0.0) layer.legendAnchorY = 0.0;
        if (layer.legendAnchorY > 1.0) layer.legendAnchorY = 1.0;
        normalizePointMetadata(layer);
    }
    if (project.settings().uiFontPercent < 80) project.settings().uiFontPercent = 80;
    if (project.settings().uiFontPercent > 200) project.settings().uiFontPercent = 200;

    if (project.settings().hasCustomViewport) {
        if (project.settings().viewXMin > project.settings().viewXMax) {
            std::swap(project.settings().viewXMin, project.settings().viewXMax);
        }
        if (project.settings().viewYMin > project.settings().viewYMax) {
            std::swap(project.settings().viewYMin, project.settings().viewYMax);
        }
        if (project.settings().viewXMin == project.settings().viewXMax) {
            project.settings().viewXMin -= 1.0;
            project.settings().viewXMax += 1.0;
        }
        if (project.settings().viewYMin == project.settings().viewYMax) {
            project.settings().viewYMin -= 1.0;
            project.settings().viewYMax += 1.0;
        }
    }
}

} // namespace

void ProjectSerializer::save(const Project& project, const std::string& path) {
    const auto tmpPath = tempSavePathFor(path);
    {
        std::ofstream out(tmpPath, std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Cannot write project: " + tmpPath);
        }

        out << "PLOTAPP_PROJECT=" << kCurrentProjectFormatVersion << '\n';
        writeKV(out, "TITLE", project.settings().title);
        writeKV(out, "XLABEL", project.settings().xLabel);
        writeKV(out, "YLABEL", project.settings().yLabel);
        writeKV(out, "SHOW_GRID", project.settings().showGrid ? "1" : "0");
        writeKV(out, "HAS_CUSTOM_VIEWPORT", project.settings().hasCustomViewport ? "1" : "0");
        writeKV(out, "VIEW_XMIN", std::to_string(project.settings().viewXMin));
        writeKV(out, "VIEW_XMAX", std::to_string(project.settings().viewXMax));
        writeKV(out, "VIEW_YMIN", std::to_string(project.settings().viewYMin));
        writeKV(out, "VIEW_YMAX", std::to_string(project.settings().viewYMax));
        writeKV(out, "UI_THEME", project.settings().uiTheme);
        writeKV(out, "UI_FONT_PERCENT", std::to_string(project.settings().uiFontPercent));

        for (const auto& layer : project.layers()) {
            out << "LAYER_BEGIN\n";
            writeKV(out, "ID", layer.id);
            writeKV(out, "NAME", layer.name);
            writeKV(out, "TYPE", layerTypeToString(layer.type));
            writeKV(out, "VISIBLE", layer.visible ? "1" : "0");
            writeKV(out, "COLOR", layer.style.color);
            writeKV(out, "COLOR2", layer.style.secondaryColor);
            writeKV(out, "LINE_WIDTH", std::to_string(layer.style.lineWidth));
            writeKV(out, "SHOW_MARKERS", layer.style.showMarkers ? "1" : "0");
            writeKV(out, "CONNECT_POINTS", layer.style.connectPoints ? "1" : "0");
            writeKV(out, "SOURCE_LAYER", layer.sourceLayerId);
            writeKV(out, "PARENT_LAYER", layer.parentLayerId);
            writeKV(out, "PLUGIN_ID", layer.generatorPluginId);
            writeKV(out, "PLUGIN_PARAMS", layer.generatorParams);
            writeKV(out, "NOTES", layer.notes);
            writeKV(out, "FORMULA", layer.formulaExpression);
            writeKV(out, "FORMULA_XMIN", std::to_string(layer.formulaXMin));
            writeKV(out, "FORMULA_XMAX", std::to_string(layer.formulaXMax));
            writeKV(out, "FORMULA_SAMPLES", std::to_string(layer.formulaSamples));
            writeKV(out, "IMPORT_SOURCE", layer.importedSourcePath);
            writeKV(out, "IMPORT_SHEET", layer.importedSheetName);
            writeKV(out, "IMPORT_X_COLUMN", std::to_string(layer.importedXColumn));
            writeKV(out, "IMPORT_Y_COLUMN", std::to_string(layer.importedYColumn));
            if (!layer.importedHeaders.empty()) writeKV(out, "TABLE_HEADERS", joinFields(layer.importedHeaders));
            for (const auto& row : layer.importedRows) {
                writeKV(out, "TABLE_ROW", joinFields(row));
            }
            writeKV(out, "LEGEND_VISIBLE", layer.legendVisible ? "1" : "0");
            writeKV(out, "LEGEND_TEXT", layer.legendText);
            writeKV(out, "LEGEND_X", std::to_string(layer.legendAnchorX));
            writeKV(out, "LEGEND_Y", std::to_string(layer.legendAnchorY));
            for (std::size_t i = 0; i < layer.points.size(); ++i) {
                out << "POINT=" << layer.points[i].x << ',' << layer.points[i].y << '\n';
                if (i < layer.errorValues.size()) out << "POINT_ERROR=" << layer.errorValues[i] << '\n';
                if (i < layer.pointRoles.size()) out << "POINT_ROLE=" << layer.pointRoles[i] << '\n';
                if (i < layer.pointVisibility.size()) out << "POINT_VISIBLE=" << layer.pointVisibility[i] << '\n';
                if (i < layer.importedRowIndices.size()) out << "POINT_SOURCE_ROW=" << layer.importedRowIndices[i] << '\n';
            }
            out << "LAYER_END\n";
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(tmpPath, path, ec);
        if (ec) {
            std::filesystem::remove(tmpPath, ec);
            throw std::runtime_error("Cannot finalize saved project: " + path);
        }
    }
}

Project ProjectSerializer::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot read project: " + path);
    }

    Project project;
    project.layers().clear();
    std::string line;
    Layer current;
    bool inLayer = false;
    bool headerSeen = false;
    std::size_t lineNumber = 0;

    while (std::getline(in, line)) {
        ++lineNumber;
        line = text::trim(line);
        if (line.empty()) continue;

        if (!headerSeen) {
            if (!text::startsWith(line, "PLOTAPP_PROJECT=")) {
                throw std::runtime_error("Invalid PlotApp project file header in: " + path);
            }
            const auto versionValue = text::unescape(line.substr(std::string("PLOTAPP_PROJECT=").size()));
            const int version = parseIntField("PLOTAPP_PROJECT", versionValue, lineNumber);
            if (version < 1 || version > kCurrentProjectFormatVersion) {
                throw std::runtime_error("Unsupported PlotApp project version: " + std::to_string(version));
            }
            headerSeen = true;
            continue;
        }

        if (line == "LAYER_BEGIN") {
            if (inLayer) {
                throw std::runtime_error("Nested LAYER_BEGIN is not allowed at line " + std::to_string(lineNumber));
            }
            current = Layer{};
            inLayer = true;
            continue;
        }
        if (line == "LAYER_END") {
            if (!inLayer) {
                throw std::runtime_error("Unexpected LAYER_END at line " + std::to_string(lineNumber));
            }
            if (current.id.empty()) current.id = makeLayerId();
            if (current.legendText.empty()) current.legendText = current.name;
            project.layers().push_back(current);
            inLayer = false;
            continue;
        }

        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        const std::string key = line.substr(0, pos);
        const std::string value = text::unescape(line.substr(pos + 1));
        if (!inLayer) {
            if (key == "TITLE") project.settings().title = value;
            else if (key == "XLABEL") project.settings().xLabel = value;
            else if (key == "YLABEL") project.settings().yLabel = value;
            else if (key == "SHOW_GRID") project.settings().showGrid = value == "1";
            else if (key == "HAS_CUSTOM_VIEWPORT") project.settings().hasCustomViewport = value == "1";
            else if (key == "VIEW_XMIN") project.settings().viewXMin = parseDoubleField(key, value, lineNumber);
            else if (key == "VIEW_XMAX") project.settings().viewXMax = parseDoubleField(key, value, lineNumber);
            else if (key == "VIEW_YMIN") project.settings().viewYMin = parseDoubleField(key, value, lineNumber);
            else if (key == "VIEW_YMAX") project.settings().viewYMax = parseDoubleField(key, value, lineNumber);
            else if (key == "UI_THEME") project.settings().uiTheme = value;
            else if (key == "UI_FONT_PERCENT") project.settings().uiFontPercent = parseIntField(key, value, lineNumber);
            continue;
        }

        if (key == "ID") current.id = value;
        else if (key == "NAME") current.name = value;
        else if (key == "TYPE") current.type = layerTypeFromString(value);
        else if (key == "VISIBLE") current.visible = value == "1";
        else if (key == "COLOR") current.style.color = value;
        else if (key == "COLOR2") current.style.secondaryColor = value;
        else if (key == "LINE_WIDTH") current.style.lineWidth = parseIntField(key, value, lineNumber);
        else if (key == "SHOW_MARKERS") current.style.showMarkers = value == "1";
        else if (key == "CONNECT_POINTS") current.style.connectPoints = value != "0";
        else if (key == "SOURCE_LAYER") current.sourceLayerId = value;
        else if (key == "PARENT_LAYER") current.parentLayerId = value;
        else if (key == "PLUGIN_ID") current.generatorPluginId = value;
        else if (key == "PLUGIN_PARAMS") current.generatorParams = value;
        else if (key == "NOTES") current.notes = value;
        else if (key == "FORMULA") current.formulaExpression = value;
        else if (key == "FORMULA_XMIN") current.formulaXMin = parseDoubleField(key, value, lineNumber);
        else if (key == "FORMULA_XMAX") current.formulaXMax = parseDoubleField(key, value, lineNumber);
        else if (key == "FORMULA_SAMPLES") current.formulaSamples = parseIntField(key, value, lineNumber);
        else if (key == "IMPORT_SOURCE") current.importedSourcePath = value;
        else if (key == "IMPORT_SHEET") current.importedSheetName = value;
        else if (key == "IMPORT_X_COLUMN") current.importedXColumn = static_cast<std::size_t>(std::max(0, parseIntField(key, value, lineNumber)));
        else if (key == "IMPORT_Y_COLUMN") current.importedYColumn = static_cast<std::size_t>(std::max(0, parseIntField(key, value, lineNumber)));
        else if (key == "TABLE_HEADERS") current.importedHeaders = splitFields(value);
        else if (key == "TABLE_ROW") current.importedRows.push_back(splitFields(value));
        else if (key == "LEGEND_VISIBLE") current.legendVisible = value == "1";
        else if (key == "LEGEND_TEXT") current.legendText = value;
        else if (key == "LEGEND_X") current.legendAnchorX = parseDoubleField(key, value, lineNumber);
        else if (key == "LEGEND_Y") current.legendAnchorY = parseDoubleField(key, value, lineNumber);
        else if (key == "POINT") {
            const auto comma = value.find(',');
            if (comma == std::string::npos) {
                throw std::runtime_error("Invalid POINT entry at line " + std::to_string(lineNumber));
            }
            current.points.push_back(Point{
                parseDoubleField("POINT.x", value.substr(0, comma), lineNumber),
                parseDoubleField("POINT.y", value.substr(comma + 1), lineNumber)
            });
        } else if (key == "POINT_ERROR") {
            current.errorValues.push_back(parseDoubleField(key, value, lineNumber));
        } else if (key == "POINT_ROLE") {
            current.pointRoles.push_back(parseIntField(key, value, lineNumber));
        } else if (key == "POINT_VISIBLE") {
            current.pointVisibility.push_back(parseIntField(key, value, lineNumber) == 0 ? 0 : 1);
        } else if (key == "POINT_SOURCE_ROW") {
            current.importedRowIndices.push_back(static_cast<std::size_t>(std::max(0, parseIntField(key, value, lineNumber))));
        }
    }

    if (!headerSeen) {
        throw std::runtime_error("Invalid PlotApp project file: missing header");
    }
    if (inLayer) {
        throw std::runtime_error("Invalid PlotApp project file: unterminated layer block");
    }

    validateLoadedProject(project);
    return project;
}

} // namespace plotapp
