#include "plotapp/SvgRenderer.h"

#include "plotapp/LayerSampler.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace plotapp {
namespace {

struct Bounds {
    double minX {0.0};
    double maxX {1.0};
    double minY {0.0};
    double maxY {1.0};
};

Bounds sanitizeBounds(Bounds bounds) {
    if (!std::isfinite(bounds.minX) || !std::isfinite(bounds.maxX)
        || !std::isfinite(bounds.minY) || !std::isfinite(bounds.maxY)) {
        return Bounds{};
    }
    if (bounds.minX > bounds.maxX) std::swap(bounds.minX, bounds.maxX);
    if (bounds.minY > bounds.maxY) std::swap(bounds.minY, bounds.maxY);
    if (bounds.minX == bounds.maxX) {
        bounds.minX -= 1.0;
        bounds.maxX += 1.0;
    }
    if (bounds.minY == bounds.maxY) {
        bounds.minY -= 1.0;
        bounds.maxY += 1.0;
    }
    return bounds;
}

std::string xmlEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

bool isSafeSvgColor(const std::string& color) {
    auto isHex = [](const std::string& value) {
        if (value.size() != 4 && value.size() != 7) return false;
        if (value.front() != '#') return false;
        return std::all_of(value.begin() + 1, value.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
    };
    if (isHex(color)) return true;
    static const std::vector<std::string> named = {
        "black", "white", "red", "green", "blue", "yellow", "cyan", "magenta", "gray", "grey",
        "orange", "purple", "brown", "pink"
    };
    return std::find(named.begin(), named.end(), color) != named.end();
}

std::string safeColor(const std::string& color, const std::string& fallback = "#1f77b4") {
    return isSafeSvgColor(color) ? color : fallback;
}


std::size_t utf8CodePointCount(const std::string& value) {
    std::size_t count = 0;
    for (unsigned char c : value) {
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            lines.push_back(current);
            current.clear();
        } else if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    lines.push_back(current);
    return lines;
}

std::string takeUtf8Prefix(const std::string& text, std::size_t maxCodePoints, std::size_t* byteCount) {
    std::size_t codePoints = 0;
    std::size_t end = 0;
    while (end < text.size() && codePoints < maxCodePoints) {
        const unsigned char c = static_cast<unsigned char>(text[end]);
        std::size_t sequenceLength = 1;
        if ((c & 0x80) == 0) sequenceLength = 1;
        else if ((c & 0xE0) == 0xC0) sequenceLength = 2;
        else if ((c & 0xF0) == 0xE0) sequenceLength = 3;
        else if ((c & 0xF8) == 0xF0) sequenceLength = 4;
        if (end + sequenceLength > text.size()) sequenceLength = 1;
        end += sequenceLength;
        ++codePoints;
    }
    *byteCount = end;
    return text.substr(0, end);
}

void appendWrappedWord(std::vector<std::string>& out, std::string& current, const std::string& word, std::size_t maxCharsPerLine) {
    if (word.empty()) return;

    const auto currentLength = utf8CodePointCount(current);
    const auto wordLength = utf8CodePointCount(word);
    if (current.empty() && wordLength <= maxCharsPerLine) {
        current = word;
        return;
    }
    if (!current.empty() && currentLength + 1 + wordLength <= maxCharsPerLine) {
        current.push_back(' ');
        current += word;
        return;
    }
    if (!current.empty()) {
        out.push_back(current);
        current.clear();
    }

    std::string remaining = word;
    while (utf8CodePointCount(remaining) > maxCharsPerLine) {
        std::size_t bytes = 0;
        out.push_back(takeUtf8Prefix(remaining, maxCharsPerLine, &bytes));
        remaining.erase(0, bytes);
    }
    current = remaining;
}

std::vector<std::string> wrapLegendText(const std::string& text, std::size_t maxCharsPerLine) {
    maxCharsPerLine = std::max<std::size_t>(8, maxCharsPerLine);
    std::vector<std::string> wrapped;
    for (const auto& sourceLine : splitLines(text)) {
        if (sourceLine.empty()) {
            wrapped.emplace_back();
            continue;
        }

        std::string current;
        std::string word;
        auto flushWord = [&]() {
            appendWrappedWord(wrapped, current, word, maxCharsPerLine);
            word.clear();
        };

        for (char c : sourceLine) {
            if (c == ' ' || c == '\t') {
                flushWord();
            } else {
                word.push_back(c);
            }
        }
        flushWord();
        if (!current.empty()) wrapped.push_back(current);
    }
    if (wrapped.empty()) wrapped.emplace_back();
    return wrapped;
}

double approximateLegendLineWidth(const std::string& line) {
    return static_cast<double>(utf8CodePointCount(line)) * 7.2;
}

double clampDouble(double value, double minimum, double maximum) {
    if (minimum > maximum) return minimum;
    return std::max(minimum, std::min(value, maximum));
}

Bounds computeBounds(const Project& project) {
    if (project.settings().hasCustomViewport) {
        return sanitizeBounds(Bounds{project.settings().viewXMin, project.settings().viewXMax,
                                     project.settings().viewYMin, project.settings().viewYMax});
    }

    Bounds bounds;
    bool first = true;
    for (const auto* layer : project.visibleLayers()) {
        for (std::size_t i = 0; i < layer->points.size(); ++i) {
            if (!pointIsVisible(*layer, i)) continue;
            const auto& point = layer->points[i];
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
            const double halfError = i < layer->errorValues.size() ? std::max(0.0, layer->errorValues[i]) * 0.5 : 0.0;
            const double lowY = point.y - halfError;
            const double highY = point.y + halfError;
            if (first) {
                bounds.minX = bounds.maxX = point.x;
                bounds.minY = lowY;
                bounds.maxY = highY;
                first = false;
            } else {
                bounds.minX = std::min(bounds.minX, point.x);
                bounds.maxX = std::max(bounds.maxX, point.x);
                bounds.minY = std::min(bounds.minY, lowY);
                bounds.maxY = std::max(bounds.maxY, highY);
            }
        }
    }
    if (first) return bounds;
    return sanitizeBounds(bounds);
}

double mapValue(double value, double minValue, double maxValue, double outMin, double outMax) {
    const double ratio = (value - minValue) / (maxValue - minValue);
    return outMin + ratio * (outMax - outMin);
}

bool shouldBreakSegment(const Point& previous, const Point& current, const Bounds& bounds) {
    const double yRange = std::max(1.0, bounds.maxY - bounds.minY);
    const double xRange = std::max(1.0, bounds.maxX - bounds.minX);
    return std::fabs(current.y - previous.y) > yRange * 2.0 || std::fabs(current.x - previous.x) > xRange * 0.25;
}

std::string buildPathData(const std::vector<Point>& points, const Bounds& bounds, double plotX, double plotY, double plotWidth, double plotHeight) {
    std::ostringstream path;
    bool haveActiveSegment = false;
    Point previous {};
    bool havePrevious = false;
    for (const auto& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            haveActiveSegment = false;
            havePrevious = false;
            continue;
        }
        const double x = mapValue(point.x, bounds.minX, bounds.maxX, plotX, plotX + plotWidth);
        const double y = mapValue(point.y, bounds.minY, bounds.maxY, plotY + plotHeight, plotY);
        if (!haveActiveSegment || (havePrevious && shouldBreakSegment(previous, point, bounds))) {
            path << 'M' << x << ',' << y;
            haveActiveSegment = true;
        } else {
            path << ' ' << 'L' << x << ',' << y;
        }
        previous = point;
        havePrevious = true;
    }
    return path.str();
}

std::string roleColorFor(const Layer& layer, PointRole role) {
    if (role == PointRole::Maximum) return safeColor(layer.style.secondaryColor, safeColor(layer.style.color));
    return safeColor(layer.style.color);
}

} // namespace

std::string SvgRenderer::renderToString(const Project& project, int width, int height) {
    constexpr int marginLeft = 90;
    constexpr int marginRight = 40;
    constexpr int marginTop = 60;
    constexpr int marginBottom = 80;
    const double plotX = marginLeft;
    const double plotY = marginTop;
    const double plotWidth = width - marginLeft - marginRight;
    const double plotHeight = height - marginTop - marginBottom;
    const auto bounds = computeBounds(project);
    const bool dark = project.settings().uiTheme == "dark";
    const std::string windowColor = dark ? "#202124" : "white";
    const std::string foreground = dark ? "#ffffff" : "#000000";
    const std::string plotBackground = dark ? "#111315" : "white";
    const std::string gridColor = dark ? "#3c4043" : "#e6e6e6";
    const std::string legendBg = dark ? "#2b2c30" : "white";

    std::ostringstream out;
    out << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << width << R"(" height=")" << height << R"(" viewBox="0 0 )" << width << ' ' << height << R"(">)";
    out << R"(<defs><clipPath id="plotapp-plot-clip"><rect x=")" << plotX << R"(" y=")" << plotY << R"(" width=")" << plotWidth << R"(" height=")" << plotHeight << R"("/></clipPath></defs>)";
    out << R"(<rect width="100%" height="100%" fill=")" << windowColor << R"("/>)";
    out << R"(<rect x=")" << plotX << R"(" y=")" << plotY << R"(" width=")" << plotWidth << R"(" height=")" << plotHeight << R"(" fill=")" << plotBackground << R"(" stroke=")" << foreground << R"(" stroke-width="2"/>)";
    out << R"(<text x=")" << (width / 2) << R"(" y="30" font-size="22" text-anchor="middle" font-family="sans-serif" fill=")" << foreground << R"(">)" << xmlEscape(project.settings().title) << R"(</text>)";

    if (project.settings().showGrid) {
        for (int i = 0; i <= 10; ++i) {
            const double x = plotX + (plotWidth / 10.0) * i;
            const double y = plotY + (plotHeight / 10.0) * i;
            out << R"(<line x1=")" << x << R"(" y1=")" << plotY << R"(" x2=")" << x << R"(" y2=")" << (plotY + plotHeight) << R"(" stroke=")" << gridColor << R"("/>)";
            out << R"(<line x1=")" << plotX << R"(" y1=")" << y << R"(" x2=")" << (plotX + plotWidth) << R"(" y2=")" << y << R"(" stroke=")" << gridColor << R"("/>)";
        }
    }

    const double axisX = std::clamp(0.0, bounds.minX, bounds.maxX);
    const double axisY = std::clamp(0.0, bounds.minY, bounds.maxY);
    const double axisScreenX = mapValue(axisX, bounds.minX, bounds.maxX, plotX, plotX + plotWidth);
    const double axisScreenY = mapValue(axisY, bounds.minY, bounds.maxY, plotY + plotHeight, plotY);
    out << R"(<line x1=")" << plotX << R"(" y1=")" << axisScreenY << R"(" x2=")" << (plotX + plotWidth) << R"(" y2=")" << axisScreenY << R"(" stroke=")" << foreground << R"(" stroke-width="2"/>)";
    out << R"(<line x1=")" << axisScreenX << R"(" y1=")" << plotY << R"(" x2=")" << axisScreenX << R"(" y2=")" << (plotY + plotHeight) << R"(" stroke=")" << foreground << R"(" stroke-width="2"/>)";
    out << R"(<text x=")" << (plotX + plotWidth / 2.0) << R"(" y=")" << (height - 20) << R"(" font-size="18" text-anchor="middle" font-family="sans-serif" fill=")" << foreground << R"(">)" << xmlEscape(project.settings().xLabel) << R"(</text>)";
    out << "<text x=\"25\" y=\"" << (plotY + plotHeight / 2.0) << "\" font-size=\"18\" text-anchor=\"middle\" font-family=\"sans-serif\" fill=\"" << foreground << "\" transform=\"rotate(-90 25 " << (plotY + plotHeight / 2.0) << ")\">" << xmlEscape(project.settings().yLabel) << "</text>";

    out << "<g clip-path=\"url(#plotapp-plot-clip)\">";
    for (const auto* layer : project.visibleLayers()) {
        const auto sampled = sampleLayerForViewport(project, *layer, bounds.minX, bounds.maxX, bounds.minY, bounds.maxY, static_cast<int>(plotWidth * 1.5));
        const auto primaryColor = safeColor(layer->style.color);
        if (layer->style.connectPoints) {
            const auto pathData = buildPathData(sampled.points, bounds, plotX, plotY, plotWidth, plotHeight);
            if (!pathData.empty()) {
                out << R"(<path fill="none" stroke=")" << primaryColor << R"(" stroke-width=")" << std::max(1, layer->style.lineWidth) << R"(" d=")" << pathData << R"("/>)";
            }
        }
        if (layer->style.showMarkers) {
            for (std::size_t i = 0; i < sampled.points.size(); ++i) {
                if (!std::isfinite(sampled.points[i].x) || !std::isfinite(sampled.points[i].y)) continue;
                const double x = mapValue(sampled.points[i].x, bounds.minX, bounds.maxX, plotX, plotX + plotWidth);
                const double y = mapValue(sampled.points[i].y, bounds.minY, bounds.maxY, plotY + plotHeight, plotY);
                const std::size_t sourceIndex = i < sampled.sourceIndices.size() ? sampled.sourceIndices[i] : i;
                out << R"(<circle cx=")" << x << R"(" cy=")" << y << R"(" r="3.5" fill=")" << roleColorFor(*layer, pointRoleAt(*layer, sourceIndex)) << R"("/>)";
            }
        }
        if (!layer->errorValues.empty()) {
            for (std::size_t i = 0; i < sampled.points.size(); ++i) {
                const std::size_t sourceIndex = i < sampled.sourceIndices.size() ? sampled.sourceIndices[i] : i;
                if (sourceIndex >= layer->errorValues.size()) continue;
                const double half = layer->errorValues[sourceIndex] * 0.5;
                const double x = mapValue(sampled.points[i].x, bounds.minX, bounds.maxX, plotX, plotX + plotWidth);
                const double topY = mapValue(sampled.points[i].y + half, bounds.minY, bounds.maxY, plotY + plotHeight, plotY);
                const double bottomY = mapValue(sampled.points[i].y - half, bounds.minY, bounds.maxY, plotY + plotHeight, plotY);
                out << R"(<line x1=")" << x << R"(" y1=")" << topY << R"(" x2=")" << x << R"(" y2=")" << bottomY << R"(" stroke=")" << primaryColor << R"(" stroke-width="1"/>)";
                out << R"(<line x1=")" << (x - 4) << R"(" y1=")" << topY << R"(" x2=")" << (x + 4) << R"(" y2=")" << topY << R"(" stroke=")" << primaryColor << R"(" stroke-width="1"/>)";
                out << R"(<line x1=")" << (x - 4) << R"(" y1=")" << bottomY << R"(" x2=")" << (x + 4) << R"(" y2=")" << bottomY << R"(" stroke=")" << primaryColor << R"(" stroke-width="1"/>)";
            }
        }
    }
    out << R"(</g>)";

    for (const auto* layer : project.visibleLayers()) {
        if (!layer->legendVisible) continue;

        const std::string legendText = layer->legendText.empty() ? layer->name : layer->legendText;
        const double maxLegendWidth = std::max(80.0, plotWidth - 8.0);
        const std::size_t maxCharsPerLine = static_cast<std::size_t>(std::max(8.0, (maxLegendWidth - 44.0) / 7.2));
        const auto legendLines = wrapLegendText(legendText, maxCharsPerLine);
        double textWidth = 0.0;
        for (const auto& line : legendLines) {
            textWidth = std::max(textWidth, approximateLegendLineWidth(line));
        }
        const double legendWidth = std::min(maxLegendWidth, std::max(170.0, 44.0 + textWidth));
        const double legendHeight = std::max(26.0, 12.0 + 18.0 * static_cast<double>(legendLines.size()));
        const double legendX = clampDouble(plotX + layer->legendAnchorX * plotWidth, plotX + 4.0, plotX + plotWidth - legendWidth - 4.0);
        const double legendY = clampDouble(plotY + layer->legendAnchorY * plotHeight, plotY + 4.0, plotY + plotHeight - legendHeight - 4.0);
        const auto primaryColor = safeColor(layer->style.color);
        const auto secondaryColor = safeColor(layer->style.secondaryColor, primaryColor);

        out << R"(<rect x=")" << legendX << R"(" y=")" << legendY << R"(" width=")" << legendWidth << R"(" height=")" << legendHeight << R"(" rx="4" ry="4" fill=")" << legendBg << R"(" stroke=")" << foreground << R"("/>)";
        const double swatchY = legendY + std::max(8.0, (legendHeight - 10.0) / 2.0);
        if (layerSupportsPointRoles(*layer) && !layer->pointRoles.empty() && secondaryColor != primaryColor) {
            out << R"(<rect x=")" << (legendX + 8) << R"(" y=")" << swatchY << R"(" width="8" height="10" fill=")" << primaryColor << R"("/>)";
            out << R"(<rect x=")" << (legendX + 18) << R"(" y=")" << swatchY << R"(" width="8" height="10" fill=")" << secondaryColor << R"("/>)";
        } else {
            out << R"(<rect x=")" << (legendX + 8) << R"(" y=")" << swatchY << R"(" width="18" height="10" fill=")" << primaryColor << R"("/>)";
        }

        const double textX = legendX + 34.0;
        const double firstBaseline = legendY + 18.0;
        out << R"(<text x=")" << textX << R"(" y=")" << firstBaseline << R"(" font-size="14" font-family="sans-serif" fill=")" << foreground << R"(">)";
        for (std::size_t i = 0; i < legendLines.size(); ++i) {
            out << R"(<tspan x=")" << textX << R"(")";
            if (i > 0) out << R"( dy="18")";
            out << R"(>)" << xmlEscape(legendLines[i]) << R"(</tspan>)";
        }
        out << R"(</text>)";
    }
    out << "</svg>";
    return out.str();
}

void SvgRenderer::renderToFile(const Project& project, const std::string& path, int width, int height) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write SVG: " + path);
    out << renderToString(project, width, height);
}

} // namespace plotapp
