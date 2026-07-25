#include "game/World.h"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mc::game {
Chunk::Chunk(ChunkCoord coord) : coord_(coord) {
    blocks_.fill(BlockId::Air);
}

BlockId Chunk::GetLocal(int x, int y, int z) const {
    if (x < 0 || x >= ChunkSize || z < 0 || z >= ChunkSize ||
        y < 0 || y >= ChunkHeight) {
        return BlockId::Air;
    }
    return blocks_[Index(x, y, z)];
}

void Chunk::SetLocal(int x, int y, int z, BlockId block) {
    if (x >= 0 && x < ChunkSize && z >= 0 && z < ChunkSize &&
        y >= 0 && y < ChunkHeight) {
        blocks_[Index(x, y, z)] = block;
    }
}

void Chunk::Generate() {
    for (int z = 0; z < ChunkSize; ++z) {
        for (int x = 0; x < ChunkSize; ++x) {
            SetLocal(x, 0, z, BlockId::Bedrock);
            for (int y = 1; y <= 5; ++y) SetLocal(x, y, z, BlockId::Stone);
            for (int y = 6; y <= 8; ++y) SetLocal(x, y, z, BlockId::Dirt);
            SetLocal(x, SurfaceY, z, BlockId::Grass);
        }
    }
}

int World::FloorDiv(int value, int divisor) {
    int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder && ((remainder < 0) != (divisor < 0))) --quotient;
    return quotient;
}

int World::PositiveMod(int value, int divisor) {
    const int remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
}

Chunk* World::FindChunk(int x, int z) {
    const auto it = chunks_.find({x, z});
    return it == chunks_.end() ? nullptr : it->second.get();
}

const Chunk* World::FindChunk(int x, int z) const {
    const auto it = chunks_.find({x, z});
    return it == chunks_.end() ? nullptr : it->second.get();
}

void World::Generate(int radiusChunks) {
    for (int z = -radiusChunks; z <= radiusChunks; ++z) {
        for (int x = -radiusChunks; x <= radiusChunks; ++x) {
            auto chunk = std::make_unique<Chunk>(ChunkCoord{x, z});
            chunk->Generate();
            chunks_.emplace(ChunkCoord{x, z}, std::move(chunk));
        }
    }
}

BlockId World::GetBlock(int x, int y, int z) const {
    if (y < 0 || y >= ChunkHeight) return BlockId::Air;
    const auto* chunk = FindChunk(FloorDiv(x, ChunkSize), FloorDiv(z, ChunkSize));
    return chunk
        ? chunk->GetLocal(PositiveMod(x, ChunkSize), y, PositiveMod(z, ChunkSize))
        : BlockId::Air;
}

bool World::CollidesAabb(DirectX::XMFLOAT3 minimum,
                         DirectX::XMFLOAT3 maximum) const {
    constexpr float epsilon = 0.0001f;
    const int minX = static_cast<int>(std::floor(minimum.x + epsilon));
    const int minY = static_cast<int>(std::floor(minimum.y + epsilon));
    const int minZ = static_cast<int>(std::floor(minimum.z + epsilon));
    const int maxX = static_cast<int>(std::floor(maximum.x - epsilon));
    const int maxY = static_cast<int>(std::floor(maximum.y - epsilon));
    const int maxZ = static_cast<int>(std::floor(maximum.z - epsilon));

    for (int y = minY; y <= maxY; ++y) {
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                if (GetBlock(x, y, z) != BlockId::Air) return true;
            }
        }
    }
    return false;
}

void World::BuildMesh(Chunk& chunk) {
    auto& output = chunk.Vertices();
    output.clear();
    output.reserve(4096);

    struct Face {
        int nx, ny, nz;
        float light;
        float positions[12];
    };

    static constexpr Face faces[] = {
        { 1, 0, 0, 0.82f, {1,0,0, 1,1,0, 1,1,1, 1,0,1}},
        {-1, 0, 0, 0.72f, {0,0,1, 0,1,1, 0,1,0, 0,0,0}},
        { 0, 1, 0, 1.00f, {0,1,1, 1,1,1, 1,1,0, 0,1,0}},
        { 0,-1, 0, 0.55f, {0,0,0, 1,0,0, 1,0,1, 0,0,1}},
        { 0, 0, 1, 0.88f, {1,0,1, 1,1,1, 0,1,1, 0,0,1}},
        { 0, 0,-1, 0.76f, {0,0,0, 0,1,0, 1,1,0, 1,0,0}}
    };
    static constexpr int order[] = {0, 1, 2, 0, 2, 3};

    const auto chunkCoord = chunk.Coord();
    for (int y = 0; y < ChunkHeight; ++y) {
        for (int z = 0; z < ChunkSize; ++z) {
            for (int x = 0; x < ChunkSize; ++x) {
                const BlockId block = chunk.GetLocal(x, y, z);
                if (block == BlockId::Air) continue;

                const int worldX = chunkCoord.x * ChunkSize + x;
                const int worldZ = chunkCoord.z * ChunkSize + z;

                const float temperature =
                    0.72f + 0.18f * std::sin(static_cast<float>(worldX) * 0.013f);
                const float humidity =
                    0.68f + 0.22f * std::cos(static_cast<float>(worldZ) * 0.011f);
                const auto climateUv = GrassClimateUv(temperature, humidity);

                for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
                    const auto& face = faces[faceIndex];
                    if (GetBlock(worldX + face.nx,
                                 y + face.ny,
                                 worldZ + face.nz) != BlockId::Air) {
                        continue;
                    }

                    int tile = 0;
                    DirectX::XMFLOAT2 vertexClimateUv{0.0f, 0.0f};

                    if (block == BlockId::Grass) {
                        if (faceIndex == 2) {
                            tile = 0; // grass_top.png
                            vertexClimateUv = climateUv;
                        } else if (faceIndex == 3) {
                            tile = 2; // dirt.png on the bottom
                        } else {
                            tile = 1; // grass_side.png
                            vertexClimateUv = climateUv;
                        }
                    } else if (block == BlockId::Dirt) {
                        tile = 2;
                    } else if (block == BlockId::Stone) {
                        tile = 3;
                    } else {
                        tile = 4;
                    }

                    constexpr float atlasWidth = 80.0f;
                    constexpr float atlasHeight = 16.0f;
                    const float u0 =
                        (static_cast<float>(tile * 16) + 0.5f) / atlasWidth;
                    const float u1 =
                        (static_cast<float>((tile + 1) * 16) - 0.5f) /
                        atlasWidth;
                    const float v0 = 0.5f / atlasHeight;
                    const float v1 = 15.5f / atlasHeight;
                    const DirectX::XMFLOAT2 uv[4] = {
                        {u0, v1}, {u0, v0}, {u1, v0}, {u1, v1}
                    };

                    for (int index : order) {
                        output.push_back({
                            {
                                static_cast<float>(worldX) + face.positions[index * 3],
                                static_cast<float>(y) + face.positions[index * 3 + 1],
                                static_cast<float>(worldZ) + face.positions[index * 3 + 2]
                            },
                            uv[index],
                            vertexClimateUv,
                            face.light
                        });
                    }
                }
            }
        }
    }

    chunk.MarkMeshed();
}

void World::BuildAllMeshes() {
    for (auto& [_, chunk] : chunks_) BuildMesh(*chunk);
}

void World::RebuildAround(int worldX, int worldZ) {
    const int chunkX = FloorDiv(worldX, ChunkSize);
    const int chunkZ = FloorDiv(worldZ, ChunkSize);
    const int coordinates[5][2] = {
        {chunkX, chunkZ},
        {chunkX - 1, chunkZ},
        {chunkX + 1, chunkZ},
        {chunkX, chunkZ - 1},
        {chunkX, chunkZ + 1}
    };

    for (const auto& coordinate : coordinates) {
        if (auto* chunk = FindChunk(coordinate[0], coordinate[1])) {
            BuildMesh(*chunk);
        }
    }
}

bool World::BreakBlock(const Int3& position) {
    if (position.y < 0 || position.y >= ChunkHeight) return false;

    const BlockId block = GetBlock(position.x, position.y, position.z);
    if (block == BlockId::Air || block == BlockId::Bedrock) return false;

    auto* chunk = FindChunk(FloorDiv(position.x, ChunkSize),
                            FloorDiv(position.z, ChunkSize));
    if (!chunk) return false;

    chunk->SetLocal(PositiveMod(position.x, ChunkSize),
                    position.y,
                    PositiveMod(position.z, ChunkSize),
                    BlockId::Air);
    RebuildAround(position.x, position.z);
    return true;
}

RayHit World::Raycast(DirectX::XMFLOAT3 origin,
                      DirectX::XMFLOAT3 direction,
                      float maxDistance) const {
    using namespace DirectX;

    XMVECTOR normalized = XMVector3Normalize(XMLoadFloat3(&direction));
    XMStoreFloat3(&direction, normalized);

    Int3 cell{
        static_cast<int>(std::floor(origin.x)),
        static_cast<int>(std::floor(origin.y)),
        static_cast<int>(std::floor(origin.z))
    };
    Int3 previous = cell;

    const int stepX = direction.x > 0.0f ? 1 : -1;
    const int stepY = direction.y > 0.0f ? 1 : -1;
    const int stepZ = direction.z > 0.0f ? 1 : -1;

    const auto delta = [](float value) {
        return std::abs(value) < 1e-7f
            ? std::numeric_limits<float>::infinity()
            : std::abs(1.0f / value);
    };
    const auto first = [](float originValue, int cellValue, int step, float directionValue) {
        if (std::abs(directionValue) < 1e-7f) {
            return std::numeric_limits<float>::infinity();
        }
        const float boundary = static_cast<float>(cellValue + (step > 0 ? 1 : 0));
        return (boundary - originValue) / directionValue;
    };

    const float deltaX = delta(direction.x);
    const float deltaY = delta(direction.y);
    const float deltaZ = delta(direction.z);

    float nextX = first(origin.x, cell.x, stepX, direction.x);
    float nextY = first(origin.y, cell.y, stepY, direction.y);
    float nextZ = first(origin.z, cell.z, stepZ, direction.z);
    float distance = 0.0f;

    for (int iteration = 0; iteration < 256 && distance <= maxDistance; ++iteration) {
        if (GetBlock(cell.x, cell.y, cell.z) != BlockId::Air) {
            return {true, cell, previous, distance};
        }

        previous = cell;
        if (nextX < nextY && nextX < nextZ) {
            cell.x += stepX;
            distance = nextX;
            nextX += deltaX;
        } else if (nextY < nextZ) {
            cell.y += stepY;
            distance = nextY;
            nextY += deltaY;
        } else {
            cell.z += stepZ;
            distance = nextZ;
            nextZ += deltaZ;
        }
    }

    return {};
}
} // namespace mc::game
