#include "shared/Crypto.h"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace mc::crypto {
namespace {
constexpr char kManifestPublicKeyBlobBase64[] = "UlNBMQAIAAADAAAAAAEAAAAAAAAAAAAAAQAB7YKSivvhojuduKlj9pxK6DoMxDWw77QLtErvdrzyLJ+kQEVXOjqhXD0qs3a2xwEp0IrmIw9ridjl9gnwF7gkE+/mIwmUirju06yxNhSdVkXp/L8R0HId0as66fe5hk4yJWGwn71yZvvHFTQftbkARZKh1T/YDz6WiKdMs0sjkF+ZuledBmbMH0M/jtuIWg2ULHY1Wq9g4ILaygMUJeX9LeI0GVGmO35zJdm3tWxDIuNZPJShBOA3vxkspR8Xt/PBEx/GkKDxh3qFY9gU313hkFvmfdAJjph5MEKdbpD6cmT2r+BGqnS9Y2E+7Olt3hzO+uAWikXncRTQ4CjhDXteNw==";

struct AlgHandle {
    BCRYPT_ALG_HANDLE value{};
    ~AlgHandle() { if (value) BCryptCloseAlgorithmProvider(value, 0); }
};
struct HashHandle {
    BCRYPT_HASH_HANDLE value{};
    ~HashHandle() { if (value) BCryptDestroyHash(value); }
};
struct KeyHandle {
    BCRYPT_KEY_HANDLE value{};
    ~KeyHandle() { if (value) BCryptDestroyKey(value); }
};

Bytes HashInternal(std::span<const std::uint8_t> bytes, std::span<const std::uint8_t> hmacKey) {
    AlgHandle alg;
    const ULONG flags = hmacKey.empty() ? 0u : BCRYPT_ALG_HANDLE_HMAC_FLAG;
    if (BCryptOpenAlgorithmProvider(&alg.value, BCRYPT_SHA256_ALGORITHM, nullptr, flags) < 0) throw std::runtime_error("BCryptOpenAlgorithmProvider failed");

    DWORD objectSize = 0, hashSize = 0, used = 0;
    if (BCryptGetProperty(alg.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &used, 0) < 0) throw std::runtime_error("BCrypt object size failed");
    if (BCryptGetProperty(alg.value, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize), &used, 0) < 0) throw std::runtime_error("BCrypt hash size failed");
    Bytes object(objectSize), result(hashSize);
    HashHandle hash;
    PUCHAR key = hmacKey.empty() ? nullptr : const_cast<PUCHAR>(hmacKey.data());
    ULONG keySize = static_cast<ULONG>(hmacKey.size());
    if (BCryptCreateHash(alg.value, &hash.value, object.data(), static_cast<ULONG>(object.size()), key, keySize, 0) < 0) throw std::runtime_error("BCryptCreateHash failed");
    if (!bytes.empty() && BCryptHashData(hash.value, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0) < 0) throw std::runtime_error("BCryptHashData failed");
    if (BCryptFinishHash(hash.value, result.data(), static_cast<ULONG>(result.size()), 0) < 0) throw std::runtime_error("BCryptFinishHash failed");
    return result;
}
} // namespace

Bytes RandomBytes(std::size_t count) {
    Bytes result(count);
    if (!result.empty() && BCryptGenRandom(nullptr, result.data(), static_cast<ULONG>(result.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) throw std::runtime_error("BCryptGenRandom failed");
    return result;
}

Bytes Sha256(std::span<const std::uint8_t> bytes) { return HashInternal(bytes, {}); }
std::string Sha256Hex(std::string_view text) {
    auto bytes = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    return HexEncode(Sha256(bytes));
}
std::string Sha256FileHex(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    AlgHandle alg;
    if (BCryptOpenAlgorithmProvider(&alg.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    DWORD objectSize = 0, hashSize = 0, used = 0;
    if (BCryptGetProperty(alg.value, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &used, 0) < 0) return {};
    if (BCryptGetProperty(alg.value, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashSize), sizeof(hashSize), &used, 0) < 0) return {};
    Bytes object(objectSize), result(hashSize);
    HashHandle hash;
    if (BCryptCreateHash(alg.value, &hash.value, object.data(), static_cast<ULONG>(object.size()), nullptr, 0, 0) < 0) return {};
    std::array<char, 64 * 1024> buffer{};
    while (file) {
        file.read(buffer.data(), buffer.size());
        const auto count = file.gcount();
        if (count > 0 && BCryptHashData(hash.value, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(count), 0) < 0) return {};
    }
    if (BCryptFinishHash(hash.value, result.data(), static_cast<ULONG>(result.size()), 0) < 0) return {};
    return HexEncode(result);
}
Bytes HmacSha256(std::span<const std::uint8_t> key, std::string_view text) {
    auto bytes = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    return HashInternal(bytes, key);
}
std::string HexEncode(std::span<const std::uint8_t> bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(bytes.size() * 2, '\0');
    for (std::size_t i = 0; i < bytes.size(); ++i) { out[i * 2] = digits[bytes[i] >> 4]; out[i * 2 + 1] = digits[bytes[i] & 15]; }
    return out;
}
Bytes HexDecode(std::string_view text) {
    if (text.size() % 2) return {};
    auto nibble = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; };
    Bytes out(text.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) { int hi = nibble(text[i * 2]), lo = nibble(text[i * 2 + 1]); if (hi < 0 || lo < 0) return {}; out[i] = static_cast<std::uint8_t>((hi << 4) | lo); }
    return out;
}
Bytes Base64Decode(std::string_view text) {
    DWORD size = 0;
    if (!CryptStringToBinaryA(text.data(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64, nullptr, &size, nullptr, nullptr)) return {};
    Bytes out(size);
    if (!CryptStringToBinaryA(text.data(), static_cast<DWORD>(text.size()), CRYPT_STRING_BASE64, out.data(), &size, nullptr, nullptr)) return {};
    out.resize(size);
    return out;
}
bool ConstantTimeEqual(std::span<const std::uint8_t> a, std::span<const std::uint8_t> b) {
    if (a.size() != b.size()) return false;
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    return diff == 0;
}
bool VerifyManifestSignature(std::string_view canonicalPayload, std::string_view base64Signature) {
    try {
        auto signature = Base64Decode(base64Signature);
        if (signature.empty()) return false;
        auto payload = std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(canonicalPayload.data()), canonicalPayload.size());
        auto digest = Sha256(payload);
        AlgHandle alg;
        if (BCryptOpenAlgorithmProvider(&alg.value, BCRYPT_RSA_ALGORITHM, nullptr, 0) < 0) return false;
        KeyHandle key;
        auto publicBlob = Base64Decode(kManifestPublicKeyBlobBase64);
        if (publicBlob.empty() || BCryptImportKeyPair(alg.value, nullptr, BCRYPT_RSAPUBLIC_BLOB, &key.value, publicBlob.data(), static_cast<ULONG>(publicBlob.size()), 0) < 0) return false;
        BCRYPT_PSS_PADDING_INFO padding{ BCRYPT_SHA256_ALGORITHM, 32 };
        return BCryptVerifySignature(key.value, &padding, digest.data(), static_cast<ULONG>(digest.size()), signature.data(), static_cast<ULONG>(signature.size()), BCRYPT_PAD_PSS) >= 0;
    } catch (...) { return false; }
}
bool ProtectForCurrentUser(std::span<const std::uint8_t> plain, Bytes& protectedBytes) {
    DATA_BLOB in{ static_cast<DWORD>(plain.size()), const_cast<BYTE*>(plain.data()) }, out{};
    if (!CryptProtectData(&in, L"MemphisCat Minecraft install key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    protectedBytes.assign(out.pbData, out.pbData + out.cbData); LocalFree(out.pbData); return true;
}
bool UnprotectForCurrentUser(std::span<const std::uint8_t> protectedBytes, Bytes& plain) {
    DATA_BLOB in{ static_cast<DWORD>(protectedBytes.size()), const_cast<BYTE*>(protectedBytes.data()) }, out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    plain.assign(out.pbData, out.pbData + out.cbData); LocalFree(out.pbData); return true;
}
std::string WideToUtf8(std::wstring_view value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size, '\0'); WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr); return out;
}
std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(size, L'\0'); MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), out.data(), size); return out;
}
} // namespace mc::crypto
