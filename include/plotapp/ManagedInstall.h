#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace plotapp {

struct ManagedInstallInfo {
    bool valid {false};
    std::filesystem::path manifestPath;
    std::string manifestVersion;
    std::string appName;
    std::string appId;
    std::string installHome;
    std::string systemManagerPath;
    std::string launcherPath;
    std::string updateLauncherPath;
    std::string installedAt;
    std::string installedVersion;
    std::string sourceCommit;
    std::string sourceCommitShort;
    std::string repoUrl;
    std::string branch;
};

struct ManagedInstallUpdateStatus {
    bool valid {false};
    std::string installedVersion;
    std::string installedCommit;
    std::string updateSource;
    std::string remoteBranch;
    std::string remoteCommit;
    std::string status;
};

std::vector<std::filesystem::path> managedInstallManifestCandidates(const std::filesystem::path& executablePath);
std::optional<std::filesystem::path> findManagedInstallManifest(const std::vector<std::filesystem::path>& candidates);
std::optional<ManagedInstallInfo> loadManagedInstallInfo(const std::filesystem::path& manifestPath);
ManagedInstallUpdateStatus parseManagedInstallUpdateStatus(const std::string& output);

} // namespace plotapp
