#include "launcher/Updater.h"
#include "shared/Crypto.h"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
std::filesystem::path ExecutableDirectory() {
    std::wstring path(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) {
        throw std::runtime_error("Could not resolve launcher path");
    }
    path.resize(size);
    return std::filesystem::path(path).parent_path();
}

void AppendFatalLog(const std::filesystem::path& logPath, const std::string& message) {
    std::error_code ec;
    std::filesystem::create_directories(logPath.parent_path(), ec);
    std::ofstream file(logPath, std::ios::binary | std::ios::app);
    if (file) file << "FATAL: " << message << '\n';
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::filesystem::path logPath;
    try {
        const auto launcherDirectory = ExecutableDirectory();
        const auto installRoot = launcherDirectory.parent_path(); // bin\..\
        logPath = installRoot / L"logs" / L"launcher.log";

        mc::launcher::Updater updater(installRoot, launcherDirectory);
        const auto remote = updater.DownloadAndValidateManifest();
        if (!updater.IsInstalledReleaseCurrent(remote)) updater.UpdateGame(remote);
        updater.StartGame();
        return 0;
    } catch (const std::exception& error) {
        if (!logPath.empty()) AppendFatalLog(logPath, error.what());

        std::wstring message = mc::crypto::Utf8ToWide(error.what());
        if (!logPath.empty()) {
            message += L"\n\nDetailed log:\n";
            message += logPath.wstring();
        }
        MessageBoxW(nullptr, message.c_str(), L"Minecraft Launcher", MB_OK | MB_ICONERROR);
        return 1;
    }
}
