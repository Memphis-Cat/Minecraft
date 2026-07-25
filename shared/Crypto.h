#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mc::crypto {
using Bytes = std::vector<std::uint8_t>;

Bytes RandomBytes(std::size_t count);
Bytes Sha256(std::span<const std::uint8_t> bytes);
std::string Sha256Hex(std::string_view text);
std::string Sha256FileHex(const std::filesystem::path& path);
Bytes HmacSha256(std::span<const std::uint8_t> key, std::string_view text);
std::string HexEncode(std::span<const std::uint8_t> bytes);
Bytes HexDecode(std::string_view text);
Bytes Base64Decode(std::string_view text);
bool ConstantTimeEqual(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b);
bool VerifyManifestSignature(std::string_view canonicalPayload, std::string_view base64Signature);
bool ProtectForCurrentUser(std::span<const std::uint8_t> plain, Bytes& protectedBytes);
bool UnprotectForCurrentUser(std::span<const std::uint8_t> protectedBytes, Bytes& plain);
std::string WideToUtf8(std::wstring_view value);
std::wstring Utf8ToWide(std::string_view value);
} // namespace mc::crypto
