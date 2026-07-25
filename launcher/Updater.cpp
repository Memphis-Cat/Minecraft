#include "launcher/Updater.h"
#include "shared/Crypto.h"
#include "shared/LaunchGate.h"

#include <windows.h>
#include <winhttp.h>
#include <shlwapi.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")

namespace mc::launcher {
namespace {
constexpr wchar_t kManifestUrl[] = L"https://raw.githubusercontent.com/Memphis-Cat/Minecraft/main/manifest.json";
constexpr wchar_t kRepositoryUrl[] = L"https://github.com/Memphis-Cat/Minecraft.git";
constexpr char kLauncherVersion[] = "1.0.1";
constexpr char kExpectedChannelKeyHash[] = "677b073a4aa1f56e0b23218903d1917e563f21a1cdf5a91df8951b568dae7d5e";

std::wstring Quote(const std::filesystem::path& value) {
    const std::wstring input = value.wstring();
    std::wstring output = L"\"";
    unsigned slashes = 0;
    for (const wchar_t character : input) {
        if (character == L'\\') {
            ++slashes;
            continue;
        }
        if (character == L'\"') {
            output.append(slashes * 2 + 1, L'\\');
            output.push_back(L'\"');
            slashes = 0;
            continue;
        }
        output.append(slashes, L'\\');
        slashes = 0;
        output.push_back(character);
    }
    output.append(slashes * 2, L'\\');
    output.push_back(L'\"');
    return output;
}

std::filesystem::path FindExecutable(const wchar_t* name) {
    wchar_t buffer[32768]{};
    const DWORD size = SearchPathW(nullptr, name, nullptr,
                                   static_cast<DWORD>(std::size(buffer)),
                                   buffer, nullptr);
    if (!size || size >= std::size(buffer)) {
        throw std::runtime_error("Required executable is missing from PATH");
    }
    return buffer;
}

std::string Timestamp() {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                  time.wYear, time.wMonth, time.wDay,
                  time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
    return buffer;
}

std::string PathUtf8(const std::filesystem::path& path) {
    return mc::crypto::WideToUtf8(path.wstring());
}

std::string Win32Failure(const char* operation, const DWORD error) {
    return std::string(operation) + " failed with Windows error " + std::to_string(error);
}
} // namespace

Updater::Updater(std::filesystem::path installRoot,
                 std::filesystem::path launcherDirectory)
    : root_(std::move(installRoot)),
      sourceDir_(root_ / L"source"),
      gameDir_(std::move(launcherDirectory)),
      gameExe_(gameDir_ / L"Minecraft.exe"),
      localManifest_(gameDir_ / L"local_manifest.json"),
      logFile_(root_ / L"logs" / L"launcher.log") {
    std::error_code error;
    std::filesystem::create_directories(logFile_.parent_path(), error);
    Log("============================================================");
    Log(std::string("Launcher version ") + kLauncherVersion + " started");
    Log("Install root: " + PathUtf8(root_));
    Log("Binary folder: " + PathUtf8(gameDir_));
    Log("Update source folder: " + PathUtf8(sourceDir_));
    Log("Update build folder: " + PathUtf8(root_ / L"update-build-game"));
}

void Updater::Log(const std::string_view message) const {
    std::error_code error;
    std::filesystem::create_directories(logFile_.parent_path(), error);
    std::ofstream file(logFile_, std::ios::binary | std::ios::app);
    if (file) file << '[' << Timestamp() << "] " << message << '\n';
}

std::wstring Updater::HttpGet(const std::wstring& url) const {
    Log("Downloading signed manifest from GitHub");

    URL_COMPONENTS parts{sizeof(parts)};
    wchar_t host[256]{};
    wchar_t path[4096]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = static_cast<DWORD>(std::size(path));
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS) {
        throw std::runtime_error("Invalid manifest URL");
    }

    HINTERNET session = WinHttpOpen(L"MemphisCatMinecraftLauncher/1.0.1",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) throw std::runtime_error(Win32Failure("WinHttpOpen", GetLastError()));

    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connection) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(session);
        throw std::runtime_error(Win32Failure("WinHttpConnect", error));
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"GET", path, nullptr,
                                           WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!request) {
        const DWORD error = GetLastError();
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        throw std::runtime_error(Win32Failure("WinHttpOpenRequest", error));
    }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirectPolicy, sizeof(redirectPolicy));

    bool success = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                   WinHttpReceiveResponse(request, nullptr);
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (success) {
        success = WinHttpQueryHeaders(request,
                                      WINHTTP_QUERY_STATUS_CODE |
                                          WINHTTP_QUERY_FLAG_NUMBER,
                                      nullptr, &status, &statusSize, nullptr) &&
                  status == 200;
    }

    std::string body;
    while (success) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            success = false;
            break;
        }
        if (!available) break;

        const std::size_t offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, body.data() + offset, available, &read)) {
            success = false;
            break;
        }
        body.resize(offset + read);
        if (body.size() > 1024 * 1024) {
            success = false;
            break;
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    Log("Manifest HTTP status: " + std::to_string(status) +
        ", bytes: " + std::to_string(body.size()));
    if (!success) throw std::runtime_error("Could not download the signed manifest");
    return mc::crypto::Utf8ToWide(body);
}

Manifest Updater::DownloadAndValidateManifest() {
    const auto wide = HttpGet(kManifestUrl);
    auto manifest = ParseManifest(mc::crypto::WideToUtf8(wide));

    if (manifest.schema != 1) throw std::runtime_error("Unsupported manifest schema");
    if (manifest.channelKeyHash != kExpectedChannelKeyHash) {
        throw std::runtime_error("Manifest channel key does not match this launcher");
    }
    if (!mc::crypto::VerifyManifestSignature(manifest.CanonicalPayload(),
                                              manifest.signature)) {
        throw std::runtime_error("Manifest signature validation failed");
    }
    if (!VersionAtLeast(kLauncherVersion, manifest.minimumLauncherVersion)) {
        throw std::runtime_error("This launcher is too old and must be rebuilt manually");
    }
    if (manifest.sourceCommit.size() != 40) {
        throw std::runtime_error("Manifest source commit is invalid");
    }

    Log("Manifest validated: version=" + manifest.version +
        ", source_commit=" + manifest.sourceCommit);
    return manifest;
}

bool Updater::IsInstalledReleaseCurrent(const Manifest& remote) const {
    try {
        if (!std::filesystem::is_regular_file(gameExe_)) {
            Log("Minecraft.exe is missing; update required");
            return false;
        }
        if (!std::filesystem::is_regular_file(localManifest_)) {
            Log("local_manifest.json is missing; update required");
            return false;
        }

        const auto local = ReadManifestFile(localManifest_);
        if (!local.SameRelease(remote)) {
            Log("Local release metadata does not match the signed remote release");
            return false;
        }
        if (local.gameExeSha256.empty()) {
            Log("Local Minecraft hash is missing; update required");
            return false;
        }

        const auto actualHash = mc::crypto::Sha256FileHex(gameExe_);
        if (actualHash != local.gameExeSha256) {
            Log("Minecraft.exe SHA-256 mismatch; expected=" + local.gameExeSha256 +
                ", actual=" + actualHash);
            return false;
        }

        Log("Installed Minecraft release and SHA-256 are current");
        return true;
    } catch (const std::exception& error) {
        Log(std::string("Installed-release check failed: ") + error.what());
        return false;
    }
}

void Updater::RunHidden(const std::filesystem::path& application,
                        const std::wstring& arguments,
                        const std::filesystem::path& workingDirectory) const {
    const std::wstring command = Quote(application) + L" " + arguments;
    Log("RUN cwd=" + PathUtf8(workingDirectory) +
        " command=" + mc::crypto::WideToUtf8(command));

    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE logHandle = CreateFileW(logFile_.c_str(), FILE_APPEND_DATA,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security, OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
    if (logHandle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(Win32Failure("Opening launcher log", GetLastError()));
    }

    HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullInput == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        CloseHandle(logHandle);
        throw std::runtime_error(Win32Failure("Opening NUL input", error));
    }

    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = nullInput;
    startup.hStdOutput = logHandle;
    startup.hStdError = logHandle;

    const BOOL created = CreateProcessW(application.c_str(), mutableCommand.data(),
                                        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                        nullptr, workingDirectory.c_str(),
                                        &startup, &process);
    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();
    CloseHandle(nullInput);
    CloseHandle(logHandle);

    if (!created) {
        const auto message = Win32Failure("CreateProcess", createError);
        Log(message);
        throw std::runtime_error(message);
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    Log("Command exit code: " + std::to_string(exitCode));
    if (exitCode != 0) {
        throw std::runtime_error("The update/build command failed with exit code " +
                                 std::to_string(exitCode) + ". See " +
                                 PathUtf8(logFile_));
    }
}

void Updater::UpdateGame(const Manifest& remote) {
    Log("Beginning game update");
    std::filesystem::create_directories(root_);
    std::filesystem::create_directories(gameDir_);

    const auto updateBuildDir = root_ / L"update-build-game";
    std::error_code cleanupError;
    std::filesystem::remove_all(updateBuildDir, cleanupError);
    if (cleanupError) {
        Log("Warning: could not fully remove stale update build folder: " +
            cleanupError.message());
    }

    const auto legacySource = gameDir_ / L"source";
    if (legacySource != sourceDir_ && std::filesystem::exists(legacySource)) {
        Log("Removing legacy source folder: " + PathUtf8(legacySource));
        std::error_code error;
        std::filesystem::remove_all(legacySource, error);
        if (error) {
            Log("Warning: could not fully remove legacy source folder: " +
                error.message());
        }
    }

    const auto git = FindExecutable(L"git.exe");
    if (!std::filesystem::is_directory(sourceDir_ / L".git")) {
        std::error_code error;
        std::filesystem::remove_all(sourceDir_, error);
        RunHidden(git,
                  L"clone --filter=blob:none --no-checkout \"" +
                      std::wstring(kRepositoryUrl) + L"\" " + Quote(sourceDir_),
                  root_);
    }

    RunHidden(git,
              L"-C " + Quote(sourceDir_) +
                  L" remote set-url origin \"" + std::wstring(kRepositoryUrl) + L"\"",
              root_);
    const auto commit = mc::crypto::Utf8ToWide(remote.sourceCommit);
    RunHidden(git, L"-C " + Quote(sourceDir_) +
                       L" fetch --depth=1 origin " + commit, root_);
    RunHidden(git, L"-C " + Quote(sourceDir_) +
                       L" checkout --force FETCH_HEAD", root_);
    RunHidden(git, L"-C " + Quote(sourceDir_) + L" clean -fdx", root_);

    const auto cmd = FindExecutable(L"cmd.exe");
    const std::wstring batchCommand =
        L"\"" + Quote(sourceDir_ / L"build_game.bat") +
        L" " + Quote(sourceDir_) +
        L" " + Quote(gameDir_) +
        L" " + Quote(updateBuildDir) + L"\"";
    RunHidden(cmd, L"/d /s /c " + batchCommand, root_);

    if (!std::filesystem::is_regular_file(gameExe_)) {
        throw std::runtime_error("Build completed without producing Minecraft.exe");
    }

    std::error_code finalCleanupError;
    std::filesystem::remove_all(updateBuildDir, finalCleanupError);
    if (finalCleanupError) {
        Log("Warning: could not remove update build folder: " +
            finalCleanupError.message());
    }

    auto local = remote;
    local.gameExeSha256 = mc::crypto::Sha256FileHex(gameExe_);
    if (local.gameExeSha256.empty()) {
        throw std::runtime_error("Could not hash the built game");
    }
    WriteLocalManifest(localManifest_, local);
    Log("Game update completed; Minecraft SHA-256=" + local.gameExeSha256);
}

void Updater::StartGame() {
    if (!std::filesystem::is_regular_file(gameExe_)) {
        throw std::runtime_error("Minecraft.exe is missing");
    }

    const auto ticket = mc::launchgate::CreateLaunchTicket(gameExe_);
    std::wstring command = Quote(gameExe_) + L" --launch-ticket " + Quote(ticket);
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(gameExe_.c_str(), mutableCommand.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, gameDir_.c_str(), &startup, &process)) {
        const DWORD error = GetLastError();
        std::error_code cleanupError;
        std::filesystem::remove(ticket, cleanupError);
        throw std::runtime_error(Win32Failure("Starting Minecraft.exe", error));
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    Log("Minecraft.exe started successfully");
}
} // namespace mc::launcher
