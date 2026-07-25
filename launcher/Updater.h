#pragma once

#include "launcher/Manifest.h"
#include <filesystem>
#include <string>

namespace mc::launcher {
class Updater {
public:
    explicit Updater(std::filesystem::path launcherRoot);
    Manifest DownloadAndValidateManifest();
    bool IsInstalledReleaseCurrent(const Manifest& remote) const;
    void UpdateGame(const Manifest& remote);
    void StartGame();
private:
    std::filesystem::path root_, sourceDir_, gameDir_, gameExe_, localManifest_;
    std::wstring HttpGet(const std::wstring& url) const;
    void RunHidden(const std::filesystem::path& application, const std::wstring& arguments, const std::filesystem::path& workingDirectory) const;
};
} // namespace mc::launcher
