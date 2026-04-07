#include "plotapp/ManagedInstall.h"

#include "plotapp/TextUtil.h"

#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>

namespace plotapp {
namespace {

std::filesystem::path defaultDataHome() {
    if (const char* xdgDataHome = std::getenv("XDG_DATA_HOME"); xdgDataHome != nullptr && *xdgDataHome != '\0') {
        return std::filesystem::path(xdgDataHome);
    }
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home) / ".local" / "share";
    }
    return {};
}

void addUniqueCandidate(std::vector<std::filesystem::path>& out,
                        std::set<std::filesystem::path>& seen,
                        const std::filesystem::path& value) {
    if (value.empty()) return;
    const auto normalized = value.lexically_normal();
    if (seen.insert(normalized).second) out.push_back(normalized);
}

std::string mapValueOrEmpty(const std::unordered_map<std::string, std::string>& values, const std::string& key) {
    auto it = values.find(key);
    if (it == values.end()) return {};
    return it->second;
}

} // namespace

std::vector<std::filesystem::path> managedInstallManifestCandidates(const std::filesystem::path& executablePath) {
    std::vector<std::filesystem::path> candidates;
    std::set<std::filesystem::path> seen;

    if (!executablePath.empty()) {
        const auto executableDir = executablePath.parent_path();
        addUniqueCandidate(candidates, seen, executableDir / ".." / ".." / ".." / "metadata" / "installation.manifest");
        addUniqueCandidate(candidates, seen, executableDir / ".." / ".." / "metadata" / "installation.manifest");
        addUniqueCandidate(candidates, seen, executableDir / ".." / "metadata" / "installation.manifest");
    }

    const auto dataHome = defaultDataHome();
    if (!dataHome.empty()) {
        addUniqueCandidate(candidates, seen, dataHome / "plotapp-install" / "metadata" / "installation.manifest");
    }

    return candidates;
}

std::optional<std::filesystem::path> findManagedInstallManifest(const std::vector<std::filesystem::path>& candidates) {
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::optional<ManagedInstallInfo> loadManagedInstallInfo(const std::filesystem::path& manifestPath) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(manifestPath, ec)) return std::nullopt;

    std::ifstream in(manifestPath);
    if (!in) return std::nullopt;

    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        const auto key = text::trim(line.substr(0, pos));
        if (key.empty()) continue;
        values[key] = line.substr(pos + 1);
    }

    ManagedInstallInfo info;
    info.valid = true;
    info.manifestPath = manifestPath;
    info.manifestVersion = mapValueOrEmpty(values, "manifest_version");
    info.appName = mapValueOrEmpty(values, "app_name");
    info.appId = mapValueOrEmpty(values, "app_id");
    info.installHome = mapValueOrEmpty(values, "install_home");
    info.systemManagerPath = mapValueOrEmpty(values, "system_manager_path");
    info.launcherPath = mapValueOrEmpty(values, "launcher_path");
    info.updateLauncherPath = mapValueOrEmpty(values, "update_launcher_path");
    info.installedAt = mapValueOrEmpty(values, "installed_at");
    info.installedVersion = mapValueOrEmpty(values, "installed_version");
    info.sourceCommit = mapValueOrEmpty(values, "source_commit");
    info.sourceCommitShort = mapValueOrEmpty(values, "source_commit_short");
    info.repoUrl = mapValueOrEmpty(values, "repo_url");
    info.branch = mapValueOrEmpty(values, "branch");

    if (info.installHome.empty()) {
        const auto installHome = manifestPath.parent_path().parent_path();
        if (!installHome.empty()) info.installHome = installHome.string();
    }
    if (info.systemManagerPath.empty() && !info.installHome.empty()) {
        info.systemManagerPath = (std::filesystem::path(info.installHome) / "system" / "desktop_manager.sh").string();
    }
    return info;
}

ManagedInstallUpdateStatus parseManagedInstallUpdateStatus(const std::string& output) {
    ManagedInstallUpdateStatus info;
    std::string line;
    std::istringstream stream(output);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        const auto key = text::trim(line.substr(0, pos));
        const auto value = text::trim(line.substr(pos + 1));
        if (key == "Installed version") info.installedVersion = value;
        else if (key == "Installed commit") info.installedCommit = value;
        else if (key == "Update source") info.updateSource = value;
        else if (key == "Remote branch") info.remoteBranch = value;
        else if (key == "Remote commit") info.remoteCommit = value;
        else if (key == "Status") info.status = value;
    }
    info.valid = !info.status.empty() || !info.remoteCommit.empty() || !info.installedCommit.empty() || !info.installedVersion.empty();
    return info;
}

} // namespace plotapp
