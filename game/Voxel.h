#pragma once

#include <DirectXMath.h>
#include <cstdint>

namespace mc::game {
constexpr int ChunkSize = 16;
constexpr int ChunkHeight = 32;
constexpr int SurfaceY = 9;
constexpr float BreakReach = 4.5f;

enum class BlockId : std::uint8_t { Air, Grass, Dirt, Stone, Bedrock };

struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 uv;
    DirectX::XMFLOAT3 tint;
    float light;
};
struct Int3 { int x{}, y{}, z{}; };
struct RayHit { bool hit{}; Int3 block{}; Int3 previous{}; float distance{}; };

inline DirectX::XMFLOAT3 GrassTint(float temperature, float humidity) {
    temperature = (temperature < 0.0f) ? 0.0f : (temperature > 1.0f ? 1.0f : temperature);
    humidity = (humidity < 0.0f) ? 0.0f : (humidity > 1.0f ? 1.0f : humidity);
    const float adjustedHumidity = humidity * temperature;
    const int pixelX = static_cast<int>((1.0f - temperature) * 255.0f);
    const int pixelY = static_cast<int>((1.0f - adjustedHumidity) * 255.0f);
    const float x = static_cast<float>(pixelX) / 255.0f;
    const float y = static_cast<float>(pixelY) / 255.0f;
    return {0.38f + 0.16f * (1.0f - y), 0.58f + 0.30f * (1.0f - x), 0.20f + 0.16f * y};
}
} // namespace mc::game
