#pragma once

#include "plotapp/PluginApi.h"
#include "plotapp/Project.h"

#include <string>
#include <vector>

namespace plotapp {

struct PluginInfo {
    std::string path;
    std::string id;
    std::string name;
    std::string description;
    std::string defaultParams;
};

struct PluginExecutionResult {
    Layer layer;
    std::string warning;
};

class PluginManager {
public:
    PluginManager();
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    void addSearchDirectory(std::string directory);
    void discover();

    const std::vector<PluginInfo>& plugins() const;
    bool hasPlugin(const std::string& pluginId) const;
    PluginExecutionResult run(const std::string& pluginId, const Layer& sourceLayer, const std::string& params) const;

private:
    struct LoadedPlugin;

    void unloadAll();

    std::vector<std::string> searchDirectories_;
    std::vector<LoadedPlugin> loaded_;
    std::vector<PluginInfo> infoCache_;
};

} // namespace plotapp
