#include "launcher/Updater.h"
#include "shared/Crypto.h"

#include <windows.h>
#include <filesystem>
#include <stdexcept>

namespace {
std::filesystem::path ExecutableDirectory() {
    std::wstring path(32768, L'\0'); DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) throw std::runtime_error("Could not resolve launcher path"); path.resize(size); return std::filesystem::path(path).parent_path();
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        mc::launcher::Updater updater(ExecutableDirectory());
        const auto remote = updater.DownloadAndValidateManifest();
        if (!updater.IsInstalledReleaseCurrent(remote)) updater.UpdateGame(remote);
        updater.StartGame();
        return 0;
    } catch (const std::exception& e) {
        auto message = mc::crypto::Utf8ToWide(e.what());
        MessageBoxW(nullptr, message.c_str(), L"Minecraft Launcher", MB_OK | MB_ICONERROR);
        return 1;
    }
}
