#pragma once

#include "game/World.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <filesystem>
#include <unordered_map>

namespace mc::game {
class Renderer {
public:
    bool Initialize(HWND window,
                    int width,
                    int height,
                    const std::filesystem::path& assetRoot);
    void Resize(int width, int height);
    void Render(const World& world,
                DirectX::XMFLOAT3 cameraPosition,
                DirectX::CXMMATRIX viewProjection,
                int renderDistanceChunks);

private:
    struct GpuChunk {
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        UINT vertexCount{};
        std::uint64_t revision{};
    };

    bool CreateBackBuffer(int width, int height);
    bool CreatePipeline();
    bool CreateAtlas(const std::filesystem::path& assetRoot);
    bool IsChunkVisible(ChunkCoord coord,
                        DirectX::CXMMATRIX viewProjection) const;

    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> matrixBuffer_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> atlas_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> grassColorMap_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    std::unordered_map<ChunkCoord, GpuChunk, ChunkCoordHash> gpuChunks_;
    int width_{};
    int height_{};
};
} // namespace mc::game
