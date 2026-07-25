#include "launcher/Updater.h"
#include "shared/Crypto.h"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {
constexpr wchar_t kLauncherMutexName[] = L"Local\\MemphisCatMinecraftLauncher.SingleInstance";

std::filesystem::path ExecutableDirectory() {
    std::wstring path(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) {
        throw std::runtime_error("Could not resolve launcher path");
    }
    path.resize(size);
    return std::filesystem::path(path).parent_path();
}

void AppendLog(const std::filesystem::path& logPath, const std::string& level,
               const std::string& message) {
    std::error_code ec;
    std::filesystem::create_directories(logPath.parent_path(), ec);
    std::ofstream file(logPath, std::ios::binary | std::ios::app);
    if (file) file << level << ": " << message << '\n';
}

class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE value = nullptr) : value_(value) {}
    ~UniqueHandle() {
        if (value_) CloseHandle(value_);
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    HANDLE get() const { return value_; }
private:
    HANDLE value_{};
};
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    std::filesystem::path logPath;
    try {
        const auto launcherDirectory = ExecutableDirectory();
        const auto installRoot = launcherDirectory.parent_path();
        logPath = installRoot / L"logs" / L"launcher.log";

        SetLastError(ERROR_SUCCESS);
        UniqueHandle singleInstance(CreateMutexW(nullptr, FALSE, kLauncherMutexName));
        if (!singleInstance.get()) {
            throw std::runtime_error("Could not create the launcher single-instance lock");
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            AppendLog(logPath, "NOTICE", "A second launcher start was blocked while an update or launch was already running");
            MessageBoxW(nullptr,
                        L"Minecraft Launcher is already running or updating.\n\n"
                        L"Wait for the current launcher to finish before opening it again.",
                        L"Minecraft Launcher", MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        mc::launcher::Updater updater(installRoot, launcherDirectory);
        const auto remote = updater.DownloadAndValidateManifest();
        if (!updater.IsInstalledReleaseCurrent(remote)) updater.UpdateGame(remote);
        updater.StartGame();
        return 0;
    } catch (const std::exception& error) {
        if (!logPath.empty()) AppendLog(logPath, "FATAL", error.what());

        std::wstring message = mc::crypto::Utf8ToWide(error.what());
        if (!logPath.empty()) {
            message += L"\n\nDetailed log:\n";
            message += logPath.wstring();
        }
        MessageBoxW(nullptr, message.c_str(), L"Minecraft Launcher", MB_OK | MB_ICONERROR);
        return 1;
    }
}
