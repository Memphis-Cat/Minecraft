#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace mc::launcher {
struct Manifest {
    int schema{};
    std::string version;
    std::string sourceCommit;
    std::string channelKeyHash;
    std::string minimumLauncherVersion;
    std::string signature;
    std::string gameExeSha256;

    std::string CanonicalPayload() const;
    bool SameRelease(const Manifest& other) const;
};

Manifest ParseManifest(std::string_view json);
Manifest ReadManifestFile(const std::filesystem::path& path);
void WriteLocalManifest(const std::filesystem::path& path, const Manifest& manifest);
bool VersionAtLeast(std::string_view actual, std::string_view required);
} // namespace mc::launcher
