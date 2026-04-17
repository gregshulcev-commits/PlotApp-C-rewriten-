#include "plotapp/LayerSampler.h"

#include "plotapp/CurveMath.h"
#include "plotapp/FormulaEvaluator.h"
#include "plotapp/TextUtil.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <string>

namespace plotapp {
namespace {

std::string getParam(const std::string& params, const std::string& key) {
    const std::string pattern = key + "=";
    const auto pos = params.find(pattern);
    if (pos == std::string::npos) return {};
    const auto start = pos + pattern.size();
    const auto end = params.find(';', start);
    return params.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

int getParamInt(const std::string& params, const std::string& key, int fallback, int minValue = 1) {
    const auto raw = getParam(params, key);
    if (raw.empty()) return fallback;
    try {
        return std::max(minValue, std::stoi(raw));
    } catch (...) {
        return fallback;
    }
}

bool pointWithinView(const Point& point, double viewXMin, double viewXMax, double viewYMin, double viewYMax) {
    return point.x >= viewXMin && point.x <= viewXMax && point.y >= viewYMin && point.y <= viewYMax;
}

const Layer* resolveSourceLayer(const Project& project, const Layer& layer) {
    if (!layer.sourceLayerId.empty()) {
        if (const auto* source = project.findLayer(layer.sourceLayerId)) return source;
    }
    if (!layer.parentLayerId.empty()) {
        if (const auto* source = project.findLayer(layer.parentLayerId)) return source;
    }
    return nullptr;
}

std::vector<Point> selectedSourcePointsForLayer(const Layer& sourceLayer, const Layer& derivedLayer) {
    if (derivedLayer.pluginSourcePointIndices.empty()) return sourceLayer.points;

    std::vector<Point> selected;
    selected.reserve(derivedLayer.pluginSourcePointIndices.size());
    for (const auto index : derivedLayer.pluginSourcePointIndices) {
        if (index < sourceLayer.points.size()) selected.push_back(sourceLayer.points[index]);
    }
    return selected;
}

SampledLayerData sampleDiscreteLayer(const Layer& layer, double viewXMin, double viewXMax, double viewYMin, double viewYMax) {
    SampledLayerData sampled;
    const bool filterToView = !layer.style.connectPoints && (layer.style.showMarkers || !layer.errorValues.empty());
    sampled.points.reserve(layer.points.size());
    sampled.sourceIndices.reserve(layer.points.size());
    for (std::size_t i = 0; i < layer.points.size(); ++i) {
        if (!pointIsVisible(layer, i)) continue;
        if (filterToView && !pointWithinView(layer.points[i], viewXMin, viewXMax, viewYMin, viewYMax)) continue;
        sampled.points.push_back(layer.points[i]);
        sampled.sourceIndices.push_back(i);
    }
    return sampled;
}

SampledLayerData sampleContinuousDerivedLayer(const Project& project, const Layer& layer,
                                              double viewXMin, double viewXMax, int samplesHint) {
    SampledLayerData sampled;
    const auto* source = resolveSourceLayer(project, layer);
    if (!source || source->points.empty()) return sampled;

    const auto effectiveSourcePoints = selectedSourcePointsForLayer(*source, layer);
    if (effectiveSourcePoints.empty()) return sampled;

    const int samples = std::clamp(std::max(128, samplesHint), 128, 8192);

    if (layer.generatorPluginId == "linear_fit") {
        const auto model = math::fitLinearModel(effectiveSourcePoints);
        sampled.points = math::sampleLinearModel(model, viewXMin, viewXMax, samples);
        return sampled;
    }

    if (layer.generatorPluginId == "newton_deg2" || layer.generatorPluginId == "newton_deg4"
        || layer.generatorPluginId == "newton_deg5" || layer.generatorPluginId == "newton_polynomial") {
        int degree = 2;
        if (layer.generatorPluginId == "newton_deg4") degree = 4;
        else if (layer.generatorPluginId == "newton_deg5") degree = 5;
        else if (layer.generatorPluginId == "newton_polynomial") degree = getParamInt(layer.generatorParams, "degree", 2, 1);
        std::vector<double> coeffs;
        if (math::fitPolynomialRegression(effectiveSourcePoints, degree, coeffs)) {
            sampled.points = math::samplePolynomial(coeffs, viewXMin, viewXMax, samples);
        }
        return sampled;
    }

    if (layer.generatorPluginId == "smooth_curve") {
        sampled.points = math::sampleSpline(effectiveSourcePoints, viewXMin, viewXMax, samples, true);
        return sampled;
    }

    return sampled;
}

} // namespace

SampledLayerData sampleLayerForViewport(const Project& project, const Layer& layer,
                                        double viewXMin, double viewXMax,
                                        double viewYMin, double viewYMax,
                                        int samplesHint) {
    if (layer.type == LayerType::FormulaSeries && !layer.formulaExpression.empty()) {
        SampledLayerData sampled;
        try {
            const int samples = std::clamp(std::max(128, samplesHint), 128, 8192);
            sampled.points = FormulaEvaluator::sample(layer.formulaExpression, viewXMin, viewXMax, samples);
        } catch (const std::exception&) {
            return sampleDiscreteLayer(layer, viewXMin, viewXMax, viewYMin, viewYMax);
        }
        return sampled;
    }

    if (layer.type == LayerType::DerivedSeries) {
        auto continuous = sampleContinuousDerivedLayer(project, layer, viewXMin, viewXMax, samplesHint);
        if (!continuous.points.empty()) return continuous;
    }

    return sampleDiscreteLayer(layer, viewXMin, viewXMax, viewYMin, viewYMax);
}

} // namespace plotapp
