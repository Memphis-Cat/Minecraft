#pragma once

#include <DirectXMath.h>
#include <algorithm>
#include <cstdint>

namespace mc::game {
constexpr int ChunkSize = 16;
constexpr int ChunkHeight = 32;
constexpr int SurfaceY = 9;
constexpr float BreakReach = 4.5f;

constexpr float PlayerRadius = 0.30f;
constexpr float PlayerHeight = 1.80f;
constexpr float PlayerEyeHeight = 1.62f;
constexpr float WalkSpeed = 4.30f;
constexpr float SprintSpeed = 5.60f;
constexpr float JumpSpeed = 8.20f;
constexpr float Gravity = 24.0f;
constexpr float TerminalVelocity = 50.0f;

enum class BlockId : std::uint8_t { Air, Grass, Dirt, Stone, Bedrock };

struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 uv;
    DirectX::XMFLOAT2 climateUv;
    float light;
};

struct Int3 {
    int x{}, y{}, z{};
};

struct RayHit {
    bool hit{};
    Int3 block{};
    Int3 previous{};
    float distance{};
};

inline DirectX::XMFLOAT2 GrassClimateUv(float temperature, float humidity) {
    // Match Minecraft's grass colormap lookup exactly.
    temperature = std::clamp(temperature, 0.0f, 1.0f);
    humidity = std::clamp(humidity, 0.0f, 1.0f);

    const float adjustedHumidity = humidity * temperature;
    const int pixelX = static_cast<int>((1.0f - temperature) * 255.0f);
    const int pixelY = static_cast<int>((1.0f - adjustedHumidity) * 255.0f);

    // Point-sample the center of the selected pixel in the 256x256 colormap.
    return {
        (static_cast<float>(pixelX) + 0.5f) / 256.0f,
        (static_cast<float>(pixelY) + 0.5f) / 256.0f
    };
}
} // namespace mc::game
