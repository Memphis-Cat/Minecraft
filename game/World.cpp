#include "game/World.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace mc::game {
Chunk::Chunk(ChunkCoord coord) : coord_(coord) { blocks_.fill(BlockId::Air); }
BlockId Chunk::GetLocal(int x, int y, int z) const { if (x < 0 || x >= ChunkSize || z < 0 || z >= ChunkSize || y < 0 || y >= ChunkHeight) return BlockId::Air; return blocks_[Index(x, y, z)]; }
void Chunk::SetLocal(int x, int y, int z, BlockId block) { if (x >= 0 && x < ChunkSize && z >= 0 && z < ChunkSize && y >= 0 && y < ChunkHeight) blocks_[Index(x, y, z)] = block; }
void Chunk::Generate() {
    for (int z = 0; z < ChunkSize; ++z) for (int x = 0; x < ChunkSize; ++x) {
        SetLocal(x, 0, z, BlockId::Bedrock);
        for (int y = 1; y <= 5; ++y) SetLocal(x, y, z, BlockId::Stone);
        for (int y = 6; y <= 8; ++y) SetLocal(x, y, z, BlockId::Dirt);
        SetLocal(x, SurfaceY, z, BlockId::Grass);
    }
}
int World::FloorDiv(int value, int divisor) { int q = value / divisor, r = value % divisor; if (r && ((r < 0) != (divisor < 0))) --q; return q; }
int World::PositiveMod(int value, int divisor) { int r = value % divisor; return r < 0 ? r + divisor : r; }
Chunk* World::FindChunk(int x, int z) { auto it = chunks_.find({x,z}); return it == chunks_.end() ? nullptr : it->second.get(); }
const Chunk* World::FindChunk(int x, int z) const { auto it = chunks_.find({x,z}); return it == chunks_.end() ? nullptr : it->second.get(); }
void World::Generate(int radiusChunks) { for (int z = -radiusChunks; z <= radiusChunks; ++z) for (int x = -radiusChunks; x <= radiusChunks; ++x) { auto chunk = std::make_unique<Chunk>(ChunkCoord{x,z}); chunk->Generate(); chunks_.emplace(ChunkCoord{x,z}, std::move(chunk)); } }
BlockId World::GetBlock(int x, int y, int z) const { if (y < 0 || y >= ChunkHeight) return BlockId::Air; const auto* chunk = FindChunk(FloorDiv(x, ChunkSize), FloorDiv(z, ChunkSize)); return chunk ? chunk->GetLocal(PositiveMod(x, ChunkSize), y, PositiveMod(z, ChunkSize)) : BlockId::Air; }

void World::BuildMesh(Chunk& chunk) {
    auto& out = chunk.Vertices(); out.clear(); out.reserve(4096);
    struct Face { int nx,ny,nz; float light; float p[12]; };
    static constexpr Face faces[] = {
        { 1,0,0,.82f,{1,0,0, 1,1,0, 1,1,1, 1,0,1}}, {-1,0,0,.72f,{0,0,1, 0,1,1, 0,1,0, 0,0,0}},
        {0,1,0,1.0f,{0,1,1, 1,1,1, 1,1,0, 0,1,0}}, {0,-1,0,.55f,{0,0,0, 1,0,0, 1,0,1, 0,0,1}},
        {0,0,1,.88f,{1,0,1, 1,1,1, 0,1,1, 0,0,1}}, {0,0,-1,.76f,{0,0,0, 0,1,0, 1,1,0, 1,0,0}}
    };
    static constexpr int order[] = {0,1,2, 0,2,3};
    const auto c = chunk.Coord();
    for (int y = 0; y < ChunkHeight; ++y) for (int z = 0; z < ChunkSize; ++z) for (int x = 0; x < ChunkSize; ++x) {
        const BlockId block = chunk.GetLocal(x,y,z); if (block == BlockId::Air) continue;
        const int wx = c.x * ChunkSize + x, wz = c.z * ChunkSize + z;
        const float temperature = 0.72f + 0.18f * std::sin(wx * 0.013f), humidity = 0.68f + 0.22f * std::cos(wz * 0.011f);
        const auto climateTint = GrassTint(temperature, humidity);
        for (int f = 0; f < 6; ++f) {
            const auto& face = faces[f]; if (GetBlock(wx + face.nx, y + face.ny, wz + face.nz) != BlockId::Air) continue;
            int tile = 0; DirectX::XMFLOAT3 tint{1,1,1};
            if (block == BlockId::Grass) { if (f == 2) { tile = 0; tint = climateTint; } else if (f == 3) tile = 2; else { tile = 1; tint = climateTint; } }
            else if (block == BlockId::Dirt) tile = 2; else if (block == BlockId::Stone) tile = 3; else tile = 4;
            const float u0 = tile / 5.0f, u1 = (tile + 1) / 5.0f;
            const DirectX::XMFLOAT2 uv[4] = {{u0,1},{u0,0},{u1,0},{u1,1}};
            for (int i : order) out.push_back({{wx + face.p[i*3], y + face.p[i*3+1], wz + face.p[i*3+2]}, uv[i], tint, face.light});
        }
    }
    chunk.MarkMeshed();
}
void World::BuildAllMeshes() { for (auto& [_, chunk] : chunks_) BuildMesh(*chunk); }
void World::RebuildAround(int worldX, int worldZ) {
    const int cx = FloorDiv(worldX, ChunkSize), cz = FloorDiv(worldZ, ChunkSize); int coords[5][2]={{cx,cz},{cx-1,cz},{cx+1,cz},{cx,cz-1},{cx,cz+1}};
    for (auto& p : coords) if (auto* c = FindChunk(p[0],p[1])) BuildMesh(*c);
}
bool World::BreakBlock(const Int3& p) {
    if (p.y < 0 || p.y >= ChunkHeight || GetBlock(p.x,p.y,p.z) == BlockId::Air || GetBlock(p.x,p.y,p.z) == BlockId::Bedrock) return false;
    auto* chunk = FindChunk(FloorDiv(p.x, ChunkSize), FloorDiv(p.z, ChunkSize)); if (!chunk) return false;
    chunk->SetLocal(PositiveMod(p.x,ChunkSize),p.y,PositiveMod(p.z,ChunkSize),BlockId::Air); RebuildAround(p.x,p.z); return true;
}
RayHit World::Raycast(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, float maxDistance) const {
    using namespace DirectX; XMVECTOR d = XMVector3Normalize(XMLoadFloat3(&direction)); XMStoreFloat3(&direction,d);
    Int3 cell{static_cast<int>(std::floor(origin.x)),static_cast<int>(std::floor(origin.y)),static_cast<int>(std::floor(origin.z))}, previous=cell;
    const int sx = direction.x > 0 ? 1 : -1, sy = direction.y > 0 ? 1 : -1, sz = direction.z > 0 ? 1 : -1;
    auto delta=[](float v){ return std::abs(v) < 1e-7f ? std::numeric_limits<float>::infinity() : std::abs(1.0f/v); };
    const float dx=delta(direction.x),dy=delta(direction.y),dz=delta(direction.z);
    auto first=[](float o,int c,int step,float v){ if (std::abs(v)<1e-7f) return std::numeric_limits<float>::infinity(); float boundary=static_cast<float>(c+(step>0?1:0)); return (boundary-o)/v; };
    float tx=first(origin.x,cell.x,sx,direction.x),ty=first(origin.y,cell.y,sy,direction.y),tz=first(origin.z,cell.z,sz,direction.z),dist=0;
    for (int i=0;i<256 && dist<=maxDistance;++i) { if (GetBlock(cell.x,cell.y,cell.z)!=BlockId::Air) return {true,cell,previous,dist}; previous=cell; if (tx<ty && tx<tz){cell.x+=sx;dist=tx;tx+=dx;} else if(ty<tz){cell.y+=sy;dist=ty;ty+=dy;} else {cell.z+=sz;dist=tz;tz+=dz;} }
    return {};
}
} // namespace mc::game
