#include "plotapp/Project.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>
#include <sstream>
#include <unordered_set>

namespace plotapp {

Project::Project() = default;

const ProjectSettings& Project::settings() const { return settings_; }
ProjectSettings& Project::settings() { return settings_; }
const std::vector<Layer>& Project::layers() const { return layers_; }
std::vector<Layer>& Project::layers() { return layers_; }

Layer& Project::createLayer(std::string name, LayerType type) {
    Layer layer;
    layer.id = makeLayerId();
    if (name.empty()) {
        std::ostringstream ss;
        ss << "Layer " << nextLayerNumber_++;
        name = ss.str();
    }
    layer.name = std::move(name);
    layer.legendText = layer.name;
    layer.type = type;
    layers_.push_back(std::move(layer));
    return layers_.back();
}

bool Project::removeLayer(const std::string& layerId) {
    std::unordered_set<std::string> toRemove {layerId};
    bool expanded = true;
    while (expanded) {
        expanded = false;
        for (const auto& layer : layers_) {
            if (!layer.parentLayerId.empty() && toRemove.count(layer.parentLayerId) && !toRemove.count(layer.id)) {
                toRemove.insert(layer.id);
                expanded = true;
            }
        }
    }

    auto it = std::remove_if(layers_.begin(), layers_.end(), [&](const Layer& layer) {
        return toRemove.count(layer.id) > 0;
    });
    bool removed = it != layers_.end();
    layers_.erase(it, layers_.end());
    return removed;
}

Layer* Project::findLayer(const std::string& layerId) {
    auto it = std::find_if(layers_.begin(), layers_.end(), [&](const Layer& layer) { return layer.id == layerId; });
    return it == layers_.end() ? nullptr : &*it;
}

const Layer* Project::findLayer(const std::string& layerId) const {
    auto it = std::find_if(layers_.begin(), layers_.end(), [&](const Layer& layer) { return layer.id == layerId; });
    return it == layers_.end() ? nullptr : &*it;
}

std::vector<const Layer*> Project::visibleLayers() const {
    std::vector<const Layer*> result;
    for (const auto& layer : layers_) {
        if (layer.visible) result.push_back(&layer);
    }
    return result;
}

std::vector<Layer*> Project::mutableVisibleLayers() {
    std::vector<Layer*> result;
    for (auto& layer : layers_) {
        if (layer.visible) result.push_back(&layer);
    }
    return result;
}

std::string layerTypeToString(LayerType type) {
    switch (type) {
    case LayerType::DerivedSeries: return "derived";
    case LayerType::FormulaSeries: return "formula";
    case LayerType::RawSeries:
    default:
        return "raw";
    }
}

LayerType layerTypeFromString(const std::string& value) {
    if (value == "derived") return LayerType::DerivedSeries;
    if (value == "formula") return LayerType::FormulaSeries;
    return LayerType::RawSeries;
}

std::string makeLayerId() {
    static std::mt19937_64 rng(static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<unsigned long long> dist;
    std::ostringstream ss;
    ss << std::hex << dist(rng);
    return ss.str();
}

} // namespace plotapp
