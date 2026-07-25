#include "game/Renderer.h"

#include <d3dcompiler.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace mc::game {
namespace {
constexpr int TileSize = 16;
constexpr int TileCount = 5;
constexpr int AtlasWidth = TileSize * TileCount;
constexpr int AtlasHeight = TileSize;

constexpr char ShaderSource[] = R"(
cbuffer Camera : register(b0) {
    row_major float4x4 viewProjection;
};

struct VSIn {
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float2 climateUv : TEXCOORD1;
    float light : TEXCOORD2;
};

struct VSOut {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float2 climateUv : TEXCOORD1;
    float light : TEXCOORD2;
};

VSOut VSMain(VSIn input) {
    VSOut output;
    output.position = mul(float4(input.position, 1.0), viewProjection);
    output.uv = input.uv;
    output.climateUv = input.climateUv;
    output.light = input.light;
    return output;
}

Texture2D atlasTexture : register(t0);
Texture2D grassColorMap : register(t1);
SamplerState pointSampler : register(s0);

float4 PSMain(VSOut input) : SV_TARGET {
    // Atlas alpha stores the grass-tint mask. Dirt, stone and bedrock use 0.
    // Grass top uses 1, while grass side uses 1 only on its green pixels.
    const float4 blockSample = atlasTexture.Sample(pointSampler, input.uv);
    const float3 climateColor =
        grassColorMap.Sample(pointSampler, input.climateUv).rgb;
    const float3 tint = lerp(float3(1.0, 1.0, 1.0),
                             climateColor,
                             blockSample.a);
    return float4(blockSample.rgb * tint * input.light, 1.0);
}
)";

struct MatrixConstants {
    DirectX::XMFLOAT4X4 viewProjection;
};

struct ImageRgba {
    UINT width{};
    UINT height{};
    std::vector<std::uint8_t> pixels;
};

bool LoadRgbaImage(IWICImagingFactory* factory,
                   const std::filesystem::path& path,
                   UINT targetWidth,
                   UINT targetHeight,
                   ImageRgba& output) {
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(
            path.c_str(), nullptr, GENERIC_READ,
            WICDecodeMetadataCacheOnLoad, &decoder))) {
        return false;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;

    UINT sourceWidth = 0;
    UINT sourceHeight = 0;
    if (FAILED(frame->GetSize(&sourceWidth, &sourceHeight))) return false;

    ComPtr<IWICBitmapSource> source;
    if (FAILED(frame.As(&source))) return false;

    const UINT width = targetWidth == 0 ? sourceWidth : targetWidth;
    const UINT height = targetHeight == 0 ? sourceHeight : targetHeight;

    ComPtr<IWICBitmapScaler> scaler;
    if (width != sourceWidth || height != sourceHeight) {
        if (FAILED(factory->CreateBitmapScaler(&scaler))) return false;
        if (FAILED(scaler->Initialize(frame.Get(), width, height,
                                      WICBitmapInterpolationModeNearestNeighbor))) {
            return false;
        }
        if (FAILED(scaler.As(&source))) return false;
    }

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter))) return false;
    if (FAILED(converter->Initialize(
            source.Get(), GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom))) {
        return false;
    }

    output.width = width;
    output.height = height;
    output.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    const UINT stride = width * 4;
    return SUCCEEDED(converter->CopyPixels(
        nullptr, stride, static_cast<UINT>(output.pixels.size()),
        output.pixels.data()));
}

ImageRgba MakeStoneFallback() {
    ImageRgba image;
    image.width = TileSize;
    image.height = TileSize;
    image.pixels.resize(TileSize * TileSize * 4);

    for (int y = 0; y < TileSize; ++y) {
        for (int x = 0; x < TileSize; ++x) {
            const int noise = ((x * 37 + y * 19 + x * y * 3) % 31) - 15;
            const std::uint8_t value =
                static_cast<std::uint8_t>(std::clamp(126 + noise, 80, 170));
            const std::size_t index =
                static_cast<std::size_t>(y * TileSize + x) * 4;
            image.pixels[index + 0] = value;
            image.pixels[index + 1] = value;
            image.pixels[index + 2] = value;
            image.pixels[index + 3] = 255;
        }
    }
    return image;
}

bool CreateShaderResource(ID3D11Device* device,
                          UINT width,
                          UINT height,
                          const std::uint8_t* pixels,
                          ComPtr<ID3D11ShaderResourceView>& output) {
    D3D11_TEXTURE2D_DESC textureDescription{};
    textureDescription.Width = width;
    textureDescription.Height = height;
    textureDescription.MipLevels = 1;
    textureDescription.ArraySize = 1;
    textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Usage = D3D11_USAGE_IMMUTABLE;
    textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = pixels;
    initialData.SysMemPitch = width * 4;

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(
            &textureDescription, &initialData, &texture))) {
        return false;
    }
    return SUCCEEDED(device->CreateShaderResourceView(
        texture.Get(), nullptr, &output));
}

void CopyAtlasTile(const ImageRgba& image,
                   int tileIndex,
                   bool tintEveryPixel,
                   bool deriveSideMask,
                   std::vector<std::uint8_t>& atlasPixels) {
    for (int y = 0; y < TileSize; ++y) {
        for (int x = 0; x < TileSize; ++x) {
            const std::size_t sourceIndex =
                static_cast<std::size_t>(y * TileSize + x) * 4;
            const std::size_t destinationIndex =
                static_cast<std::size_t>(
                    y * AtlasWidth + tileIndex * TileSize + x) * 4;

            std::uint8_t red = image.pixels[sourceIndex + 0];
            std::uint8_t green = image.pixels[sourceIndex + 1];
            std::uint8_t blue = image.pixels[sourceIndex + 2];

            bool tintPixel = tintEveryPixel;
            if (deriveSideMask) {
                tintPixel =
                    static_cast<int>(green) > static_cast<int>(red) + 8 &&
                    static_cast<int>(green) > static_cast<int>(blue) + 16;
            }

            if (tintPixel) {
                // Convert the source's pre-colored grass pixels to luminance.
                // The climate map then supplies the actual biome color.
                const int luminance =
                    (54 * red + 183 * green + 19 * blue) / 256;
                red = green = blue =
                    static_cast<std::uint8_t>(std::clamp(luminance, 0, 255));
            }

            atlasPixels[destinationIndex + 0] = red;
            atlasPixels[destinationIndex + 1] = green;
            atlasPixels[destinationIndex + 2] = blue;
            atlasPixels[destinationIndex + 3] = tintPixel ? 255 : 0;
        }
    }
}
} // namespace

bool Renderer::Initialize(HWND window,
                          int width,
                          int height,
                          const std::filesystem::path& assetRoot) {
    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Width = width;
    description.BufferDesc.Height = height;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel{};
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    if (FAILED(D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            nullptr, 0, D3D11_SDK_VERSION, &description,
            &swapChain_, &device_, &featureLevel, &context_))) {
        return false;
    }

    return CreateBackBuffer(width, height) &&
           CreatePipeline() &&
           CreateAtlas(assetRoot);
}

bool Renderer::CreateBackBuffer(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);

    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        return false;
    }
    if (FAILED(device_->CreateRenderTargetView(
            backBuffer.Get(), nullptr, &rtv_))) {
        return false;
    }

    D3D11_TEXTURE2D_DESC depthDescription{};
    depthDescription.Width = static_cast<UINT>(width_);
    depthDescription.Height = static_cast<UINT>(height_);
    depthDescription.MipLevels = 1;
    depthDescription.ArraySize = 1;
    depthDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDescription.SampleDesc.Count = 1;
    depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    if (FAILED(device_->CreateTexture2D(
            &depthDescription, nullptr, &depth_))) {
        return false;
    }
    return SUCCEEDED(device_->CreateDepthStencilView(
        depth_.Get(), nullptr, &dsv_));
}

void Renderer::Resize(int width, int height) {
    if (!swapChain_ || width <= 0 || height <= 0) return;

    context_->OMSetRenderTargets(0, nullptr, nullptr);
    rtv_.Reset();
    dsv_.Reset();
    depth_.Reset();

    if (SUCCEEDED(swapChain_->ResizeBuffers(
            0, width, height, DXGI_FORMAT_UNKNOWN, 0))) {
        CreateBackBuffer(width, height);
    }
}

bool Renderer::CreatePipeline() {
    ComPtr<ID3DBlob> vertexShaderBlob;
    ComPtr<ID3DBlob> pixelShaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    if (FAILED(D3DCompile(
            ShaderSource, sizeof(ShaderSource), nullptr, nullptr, nullptr,
            "VSMain", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
            &vertexShaderBlob, &errorBlob))) {
        return false;
    }
    if (FAILED(D3DCompile(
            ShaderSource, sizeof(ShaderSource), nullptr, nullptr, nullptr,
            "PSMain", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
            &pixelShaderBlob, &errorBlob))) {
        return false;
    }

    if (FAILED(device_->CreateVertexShader(
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(), nullptr, &vs_))) {
        return false;
    }
    if (FAILED(device_->CreatePixelShader(
            pixelShaderBlob->GetBufferPointer(),
            pixelShaderBlob->GetBufferSize(), nullptr, &ps_))) {
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
         offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
         offsetof(Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0,
         offsetof(Vertex, climateUv), D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 2, DXGI_FORMAT_R32_FLOAT, 0,
         offsetof(Vertex, light), D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    if (FAILED(device_->CreateInputLayout(
            inputLayout, static_cast<UINT>(std::size(inputLayout)),
            vertexShaderBlob->GetBufferPointer(),
            vertexShaderBlob->GetBufferSize(), &layout_))) {
        return false;
    }

    D3D11_BUFFER_DESC constantBufferDescription{};
    constantBufferDescription.ByteWidth = sizeof(MatrixConstants);
    constantBufferDescription.Usage = D3D11_USAGE_DYNAMIC;
    constantBufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantBufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(
            &constantBufferDescription, nullptr, &matrixBuffer_))) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizerDescription{};
    rasterizerDescription.FillMode = D3D11_FILL_SOLID;
    rasterizerDescription.CullMode = D3D11_CULL_NONE;
    rasterizerDescription.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(
            &rasterizerDescription, &rasterizer_))) {
        return false;
    }

    D3D11_SAMPLER_DESC samplerDescription{};
    samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(device_->CreateSamplerState(
        &samplerDescription, &sampler_));
}

bool Renderer::CreateAtlas(const std::filesystem::path& assetRoot) {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory)))) {
        return false;
    }

    const auto blockDirectory = assetRoot / L"textures" / L"blocks";

    ImageRgba grassTop;
    ImageRgba grassSide;
    ImageRgba dirt;
    ImageRgba stone;
    ImageRgba bedrock;
    ImageRgba grassMap;

    if (!LoadRgbaImage(factory.Get(), blockDirectory / L"grass_top.png",
                       TileSize, TileSize, grassTop) ||
        !LoadRgbaImage(factory.Get(), blockDirectory / L"grass_side.png",
                       TileSize, TileSize, grassSide) ||
        !LoadRgbaImage(factory.Get(), blockDirectory / L"dirt.png",
                       TileSize, TileSize, dirt) ||
        !LoadRgbaImage(factory.Get(), blockDirectory / L"bedrock.png",
                       TileSize, TileSize, bedrock) ||
        !LoadRgbaImage(factory.Get(), blockDirectory / L"grass.png",
                       256, 256, grassMap)) {
        return false;
    }

    if (!LoadRgbaImage(factory.Get(), blockDirectory / L"stone.png",
                       TileSize, TileSize, stone)) {
        stone = MakeStoneFallback();
    }

    std::vector<std::uint8_t> atlasPixels(
        static_cast<std::size_t>(AtlasWidth) * AtlasHeight * 4);

    CopyAtlasTile(grassTop, 0, true, false, atlasPixels);
    CopyAtlasTile(grassSide, 1, false, true, atlasPixels);
    CopyAtlasTile(dirt, 2, false, false, atlasPixels);
    CopyAtlasTile(stone, 3, false, false, atlasPixels);
    CopyAtlasTile(bedrock, 4, false, false, atlasPixels);

    return CreateShaderResource(
               device_.Get(), AtlasWidth, AtlasHeight,
               atlasPixels.data(), atlas_) &&
           CreateShaderResource(
               device_.Get(), grassMap.width, grassMap.height,
               grassMap.pixels.data(), grassColorMap_);
}

bool Renderer::IsChunkVisible(
        ChunkCoord coordinate,
        DirectX::CXMMATRIX matrix) const {
    DirectX::XMFLOAT4X4 storedMatrix;
    DirectX::XMStoreFloat4x4(&storedMatrix, matrix);

    std::array<DirectX::XMFLOAT4, 6> planes = {{
        {storedMatrix._14 + storedMatrix._11,
         storedMatrix._24 + storedMatrix._21,
         storedMatrix._34 + storedMatrix._31,
         storedMatrix._44 + storedMatrix._41},
        {storedMatrix._14 - storedMatrix._11,
         storedMatrix._24 - storedMatrix._21,
         storedMatrix._34 - storedMatrix._31,
         storedMatrix._44 - storedMatrix._41},
        {storedMatrix._14 + storedMatrix._12,
         storedMatrix._24 + storedMatrix._22,
         storedMatrix._34 + storedMatrix._32,
         storedMatrix._44 + storedMatrix._42},
        {storedMatrix._14 - storedMatrix._12,
         storedMatrix._24 - storedMatrix._22,
         storedMatrix._34 - storedMatrix._32,
         storedMatrix._44 - storedMatrix._42},
        {storedMatrix._13,
         storedMatrix._23,
         storedMatrix._33,
         storedMatrix._43},
        {storedMatrix._14 - storedMatrix._13,
         storedMatrix._24 - storedMatrix._23,
         storedMatrix._34 - storedMatrix._33,
         storedMatrix._44 - storedMatrix._43}
    }};

    const float minX = static_cast<float>(coordinate.x * ChunkSize);
    const float maxX = minX + static_cast<float>(ChunkSize);
    const float minY = 0.0f;
    const float maxY = static_cast<float>(ChunkHeight);
    const float minZ = static_cast<float>(coordinate.z * ChunkSize);
    const float maxZ = minZ + static_cast<float>(ChunkSize);

    for (auto plane : planes) {
        const float length = std::sqrt(
            plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        if (length > 0.0f) {
            plane.x /= length;
            plane.y /= length;
            plane.z /= length;
            plane.w /= length;
        }

        const float x = plane.x >= 0.0f ? maxX : minX;
        const float y = plane.y >= 0.0f ? maxY : minY;
        const float z = plane.z >= 0.0f ? maxZ : minZ;
        if (plane.x * x + plane.y * y + plane.z * z + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

void Renderer::Render(const World& world,
                      DirectX::XMFLOAT3 cameraPosition,
                      DirectX::CXMMATRIX viewProjection,
                      int renderDistanceChunks) {
    const float clearColor[] = {0.50f, 0.72f, 0.95f, 1.0f};
    context_->OMSetRenderTargets(
        1, rtv_.GetAddressOf(), dsv_.Get());
    context_->ClearRenderTargetView(rtv_.Get(), clearColor);
    context_->ClearDepthStencilView(
        dsv_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        1.0f, 0);

    D3D11_VIEWPORT viewport{
        0.0f, 0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
        0.0f, 1.0f
    };
    context_->RSSetViewports(1, &viewport);
    context_->RSSetState(rasterizer_.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context_->Map(
            matrixBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        MatrixConstants constants{};
        DirectX::XMStoreFloat4x4(
            &constants.viewProjection, viewProjection);
        std::memcpy(mapped.pData, &constants, sizeof(constants));
        context_->Unmap(matrixBuffer_.Get(), 0);
    }

    context_->IASetInputLayout(layout_.Get());
    context_->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->VSSetConstantBuffers(
        0, 1, matrixBuffer_.GetAddressOf());
    context_->PSSetShader(ps_.Get(), nullptr, 0);

    ID3D11ShaderResourceView* textures[] = {
        atlas_.Get(), grassColorMap_.Get()
    };
    context_->PSSetShaderResources(0, 2, textures);
    context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());

    const int cameraChunkX =
        static_cast<int>(std::floor(cameraPosition.x / ChunkSize));
    const int cameraChunkZ =
        static_cast<int>(std::floor(cameraPosition.z / ChunkSize));

    for (const auto& [coordinate, chunk] : world.Chunks()) {
        if (std::abs(coordinate.x - cameraChunkX) > renderDistanceChunks ||
            std::abs(coordinate.z - cameraChunkZ) > renderDistanceChunks ||
            !IsChunkVisible(coordinate, viewProjection) ||
            chunk->Vertices().empty()) {
            continue;
        }

        auto& gpuChunk = gpuChunks_[coordinate];
        if (gpuChunk.revision != chunk->Revision()) {
            D3D11_BUFFER_DESC bufferDescription{};
            bufferDescription.ByteWidth = static_cast<UINT>(
                chunk->Vertices().size() * sizeof(Vertex));
            bufferDescription.Usage = D3D11_USAGE_DEFAULT;
            bufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA initialData{};
            initialData.pSysMem = chunk->Vertices().data();

            gpuChunk.vertexBuffer.Reset();
            if (FAILED(device_->CreateBuffer(
                    &bufferDescription, &initialData,
                    &gpuChunk.vertexBuffer))) {
                continue;
            }
            gpuChunk.vertexCount =
                static_cast<UINT>(chunk->Vertices().size());
            gpuChunk.revision = chunk->Revision();
        }

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        ID3D11Buffer* vertexBuffer = gpuChunk.vertexBuffer.Get();
        context_->IASetVertexBuffers(
            0, 1, &vertexBuffer, &stride, &offset);
        context_->Draw(gpuChunk.vertexCount, 0);
    }

    swapChain_->Present(1, 0);
}
} // namespace mc::game
