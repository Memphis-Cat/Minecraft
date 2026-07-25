#include "shared/LaunchGate.h"
#include "shared/Crypto.h"

#include <windows.h>
#include <shlobj.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace mc::launchgate {
namespace {
std::filesystem::path DataDirectory() {
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) throw std::runtime_error("LocalAppData is unavailable");
    auto dir = std::filesystem::path(path) / L"MemphisCatMinecraft";
    std::filesystem::create_directories(dir);
    return dir;
}
std::vector<std::uint8_t> ReadBinary(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary); if (!f) return {};
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
void WriteBinaryAtomically(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    auto tmp = path; tmp += L".tmp";
    { std::ofstream f(tmp, std::ios::binary | std::ios::trunc); if (!f) throw std::runtime_error("Cannot write install key"); f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); }
    std::error_code ec; std::filesystem::remove(path, ec); std::filesystem::rename(tmp, path);
}
std::vector<std::uint8_t> InstallSecret() {
    const auto path = DataDirectory() / L"install.secret";
    auto protectedBytes = ReadBinary(path), secret = std::vector<std::uint8_t>{};
    if (!protectedBytes.empty() && mc::crypto::UnprotectForCurrentUser(protectedBytes, secret) && secret.size() == 32) return secret;
    secret = mc::crypto::RandomBytes(32);
    if (!mc::crypto::ProtectForCurrentUser(secret, protectedBytes)) throw std::runtime_error("DPAPI could not protect the install key");
    WriteBinaryAtomically(path, protectedBytes);
    return secret;
}
std::string NormalizedGamePath(const std::filesystem::path& path) {
    std::error_code ec;
    auto absolute = std::filesystem::weakly_canonical(path, ec);
    if (ec) absolute = std::filesystem::absolute(path, ec);
    auto value = mc::crypto::WideToUtf8(absolute.wstring());
    for (char& c : value) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return value;
}
std::int64_t UnixTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
}

std::filesystem::path CreateLaunchTicket(const std::filesystem::path& gameExecutable) {
    const auto secret = InstallSecret();
    const auto nonce = mc::crypto::HexEncode(mc::crypto::RandomBytes(24));
    const auto timestamp = std::to_string(UnixTime());
    const auto message = nonce + "|" + timestamp + "|" + NormalizedGamePath(gameExecutable);
    const auto token = mc::crypto::HexEncode(mc::crypto::HmacSha256(secret, message));
    const auto path = DataDirectory() / (L"launch-" + mc::crypto::Utf8ToWide(nonce) + L".ticket");
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("Could not create the one-time launch ticket");
    file << "MCTICKET1\n" << timestamp << '\n' << nonce << '\n' << token << '\n';
    return path;
}

bool ValidateAndConsumeLaunchTicket(const std::filesystem::path& ticketPath, const std::filesystem::path& gameExecutable, std::wstring& error) {
    std::ifstream file(ticketPath, std::ios::binary);
    if (!file) { error = L"Minecraft must be started by Launcher.exe."; return false; }
    std::string magic, timestampText, nonce, token;
    std::getline(file, magic); std::getline(file, timestampText); std::getline(file, nonce); std::getline(file, token);
    file.close();
    std::error_code ec; std::filesystem::remove(ticketPath, ec);
    if (magic != "MCTICKET1" || nonce.size() != 48 || token.size() != 64) { error = L"The launcher ticket is malformed."; return false; }
    std::int64_t timestamp = 0;
    try { timestamp = std::stoll(timestampText); } catch (...) { error = L"The launcher ticket timestamp is invalid."; return false; }
    const auto now = UnixTime();
    if (timestamp < now - 30 || timestamp > now + 5) { error = L"The launcher ticket expired."; return false; }
    const auto secret = InstallSecret();
    const auto message = nonce + "|" + timestampText + "|" + NormalizedGamePath(gameExecutable);
    const auto expected = mc::crypto::HmacSha256(secret, message);
    const auto received = mc::crypto::HexDecode(token);
    if (!mc::crypto::ConstantTimeEqual(expected, received)) { error = L"The launcher ticket signature is invalid."; return false; }
    return true;
}
} // namespace mc::launchgate
