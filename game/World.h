#pragma once

#include "game/Voxel.h"
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace mc::game {
struct ChunkCoord { int x{}, z{}; bool operator==(const ChunkCoord&) const = default; };
struct ChunkCoordHash { std::size_t operator()(const ChunkCoord& c) const noexcept { return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32) ^ static_cast<std::uint32_t>(c.z); } };

class Chunk {
public:
    explicit Chunk(ChunkCoord coord);
    BlockId GetLocal(int x, int y, int z) const;
    void SetLocal(int x, int y, int z, BlockId block);
    void Generate();
    ChunkCoord Coord() const { return coord_; }
    std::vector<Vertex>& Vertices() { return vertices_; }
    const std::vector<Vertex>& Vertices() const { return vertices_; }
    std::uint64_t Revision() const { return revision_; }
    void MarkMeshed() { ++revision_; }
private:
    static std::size_t Index(int x, int y, int z) { return static_cast<std::size_t>((y * ChunkSize + z) * ChunkSize + x); }
    ChunkCoord coord_;
    std::array<BlockId, ChunkSize * ChunkHeight * ChunkSize> blocks_{};
    std::vector<Vertex> vertices_;
    std::uint64_t revision_{1};
};

class World {
public:
    void Generate(int radiusChunks);
    BlockId GetBlock(int x, int y, int z) const;
    bool BreakBlock(const Int3& position);
    RayHit Raycast(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, float maxDistance) const;
    void BuildAllMeshes();
    const std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash>& Chunks() const { return chunks_; }
private:
    Chunk* FindChunk(int chunkX, int chunkZ);
    const Chunk* FindChunk(int chunkX, int chunkZ) const;
    void BuildMesh(Chunk& chunk);
    void RebuildAround(int worldX, int worldZ);
    static int FloorDiv(int value, int divisor);
    static int PositiveMod(int value, int divisor);
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks_;
};
} // namespace mc::game
