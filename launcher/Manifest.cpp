#include "launcher/Manifest.h"

#include <array>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace mc::launcher {
namespace {
std::size_t ValueStart(std::string_view json, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    auto pos = json.find(needle); if (pos == std::string_view::npos) throw std::runtime_error("Manifest is missing " + std::string(key));
    pos = json.find(':', pos + needle.size()); if (pos == std::string_view::npos) throw std::runtime_error("Manifest syntax error");
    do { ++pos; } while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n'));
    return pos;
}
std::string StringValue(std::string_view json, std::string_view key, bool required = true) {
    try {
        auto pos = ValueStart(json, key); if (pos >= json.size() || json[pos] != '"') throw std::runtime_error("Expected string"); ++pos;
        std::string out;
        while (pos < json.size()) { char c = json[pos++]; if (c == '"') return out; if (c == '\\') { if (pos >= json.size()) break; char e = json[pos++]; if (e == '"' || e == '\\' || e == '/') out.push_back(e); else if (e == 'n') out.push_back('\n'); else if (e == 'r') out.push_back('\r'); else if (e == 't') out.push_back('\t'); else throw std::runtime_error("Unsupported JSON escape"); } else out.push_back(c); }
        throw std::runtime_error("Unterminated string");
    } catch (...) { if (!required) return {}; throw; }
}
int IntValue(std::string_view json, std::string_view key) {
    auto pos = ValueStart(json, key), end = pos;
    while (end < json.size() && json[end] >= '0' && json[end] <= '9') ++end;
    if (end == pos) throw std::runtime_error("Expected integer");
    return std::stoi(std::string(json.substr(pos, end - pos)));
}
std::string Escape(std::string_view s) {
    std::string out; out.reserve(s.size() + 8);
    for (char c : s) { if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); } else if (c == '\n') out += "\\n"; else out.push_back(c); }
    return out;
}
std::array<int, 3> ParseVersion(std::string_view v) {
    std::array<int, 3> out{}; std::size_t start = 0;
    for (int i = 0; i < 3; ++i) { auto end = v.find('.', start); auto piece = v.substr(start, end == std::string_view::npos ? v.size() - start : end - start); if (piece.empty()) throw std::runtime_error("Invalid version"); out[i] = std::stoi(std::string(piece)); if (end == std::string_view::npos) { if (i != 2) throw std::runtime_error("Invalid version"); break; } start = end + 1; }
    return out;
}
}

std::string Manifest::CanonicalPayload() const {
    return "schema=" + std::to_string(schema) + "\nversion=" + version + "\nsource_commit=" + sourceCommit + "\nchannel_key_hash=" + channelKeyHash + "\nminimum_launcher_version=" + minimumLauncherVersion + "\n";
}
bool Manifest::SameRelease(const Manifest& other) const {
    return schema == other.schema && version == other.version && sourceCommit == other.sourceCommit && channelKeyHash == other.channelKeyHash && minimumLauncherVersion == other.minimumLauncherVersion && signature == other.signature;
}
Manifest ParseManifest(std::string_view json) {
    Manifest m; m.schema = IntValue(json, "schema"); m.version = StringValue(json, "version"); m.sourceCommit = StringValue(json, "source_commit"); m.channelKeyHash = StringValue(json, "channel_key_hash"); m.minimumLauncherVersion = StringValue(json, "minimum_launcher_version"); m.signature = StringValue(json, "signature"); m.gameExeSha256 = StringValue(json, "game_exe_sha256", false); return m;
}
Manifest ReadManifestFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary); if (!file) throw std::runtime_error("Could not read local manifest");
    std::string text((std::istreambuf_iterator<char>(file)), {}); return ParseManifest(text);
}
void WriteLocalManifest(const std::filesystem::path& path, const Manifest& m) {
    std::filesystem::create_directories(path.parent_path()); auto tmp = path; tmp += L".tmp";
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc); if (!f) throw std::runtime_error("Could not write local manifest");
    f << "{\n  \"schema\": " << m.schema << ",\n  \"version\": \"" << Escape(m.version) << "\",\n  \"source_commit\": \"" << Escape(m.sourceCommit) << "\",\n  \"channel_key_hash\": \"" << Escape(m.channelKeyHash) << "\",\n  \"minimum_launcher_version\": \"" << Escape(m.minimumLauncherVersion) << "\",\n  \"signature\": \"" << Escape(m.signature) << "\",\n  \"game_exe_sha256\": \"" << Escape(m.gameExeSha256) << "\"\n}\n";
    f.close(); std::error_code ec; std::filesystem::remove(path, ec); std::filesystem::rename(tmp, path);
}
bool VersionAtLeast(std::string_view actual, std::string_view required) { return ParseVersion(actual) >= ParseVersion(required); }
} // namespace mc::launcher
