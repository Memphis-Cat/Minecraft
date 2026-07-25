#pragma once

#include "launcher/Manifest.h"
#include <filesystem>
#include <string>
#include <string_view>

namespace mc::launcher {
class Updater {
public:
    Updater(std::filesystem::path installRoot, std::filesystem::path launcherDirectory);
    Manifest DownloadAndValidateManifest();
    bool IsInstalledReleaseCurrent(const Manifest& remote) const;
    void UpdateGame(const Manifest& remote);
    void StartGame();
    const std::filesystem::path& LogPath() const noexcept { return logFile_; }

private:
    std::filesystem::path root_;
    std::filesystem::path sourceDir_;
    std::filesystem::path gameDir_;
    std::filesystem::path gameExe_;
    std::filesystem::path localManifest_;
    std::filesystem::path logFile_;

    void Log(std::string_view message) const;
    std::wstring HttpGet(const std::wstring& url) const;
    void RunHidden(const std::filesystem::path& application,
                   const std::wstring& arguments,
                   const std::filesystem::path& workingDirectory) const;
};
} // namespace mc::launcher
