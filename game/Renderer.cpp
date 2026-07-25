#include "game/Renderer.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace mc::game {
namespace {
constexpr char ShaderSource[] = R"(
cbuffer Camera : register(b0) { row_major float4x4 viewProjection; };
struct VSIn { float3 position : POSITION; float2 uv : TEXCOORD0; float3 tint : COLOR0; float light : TEXCOORD1; };
struct VSOut { float4 position : SV_POSITION; float2 uv : TEXCOORD0; float3 tint : COLOR0; float light : TEXCOORD1; };
VSOut VSMain(VSIn i) { VSOut o; o.position = mul(float4(i.position,1), viewProjection); o.uv=i.uv; o.tint=i.tint; o.light=i.light; return o; }
Texture2D atlasTexture : register(t0); SamplerState atlasSampler : register(s0);
float4 PSMain(VSOut i) : SV_TARGET { float4 c=atlasTexture.Sample(atlasSampler,i.uv); return float4(c.rgb*i.tint*i.light,c.a); }
)";
struct MatrixConstants { DirectX::XMFLOAT4X4 viewProjection; };
void PaintTile(std::vector<std::uint32_t>& pixels, int tile, std::uint32_t base, std::uint32_t fleck) {
    constexpr int tileSize=16, width=80; for(int y=0;y<tileSize;++y) for(int x=0;x<tileSize;++x) pixels[y*width+tile*tileSize+x]=(((x*13+y*7+tile*11)%17)<3)?fleck:base;
}
}

bool Renderer::Initialize(HWND window, int width, int height) {
    DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferCount=2; desc.BufferDesc.Width=width; desc.BufferDesc.Height=height; desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow=window; desc.SampleDesc.Count=1; desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL level{}; UINT flags=0;
#ifdef _DEBUG
    flags|=D3D11_CREATE_DEVICE_DEBUG;
#endif
    if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&desc,&swapChain_,&device_,&level,&context_))) return false;
    return CreateBackBuffer(width,height)&&CreatePipeline()&&CreateAtlas();
}
bool Renderer::CreateBackBuffer(int width,int height) {
    width_=std::max(1,width);height_=std::max(1,height); ComPtr<ID3D11Texture2D> back; if(FAILED(swapChain_->GetBuffer(0,IID_PPV_ARGS(&back)))) return false; if(FAILED(device_->CreateRenderTargetView(back.Get(),nullptr,&rtv_))) return false;
    D3D11_TEXTURE2D_DESC d{}; d.Width=width_;d.Height=height_;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_DEPTH_STENCIL; if(FAILED(device_->CreateTexture2D(&d,nullptr,&depth_))) return false; return SUCCEEDED(device_->CreateDepthStencilView(depth_.Get(),nullptr,&dsv_));
}
void Renderer::Resize(int width,int height) { if(!swapChain_||width<=0||height<=0)return; context_->OMSetRenderTargets(0,nullptr,nullptr);rtv_.Reset();dsv_.Reset();depth_.Reset();if(SUCCEEDED(swapChain_->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0)))CreateBackBuffer(width,height); }
bool Renderer::CreatePipeline() {
    ComPtr<ID3DBlob> vsBlob,psBlob,error; if(FAILED(D3DCompile(ShaderSource,sizeof(ShaderSource),nullptr,nullptr,nullptr,"VSMain","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vsBlob,&error)))return false; if(FAILED(D3DCompile(ShaderSource,sizeof(ShaderSource),nullptr,nullptr,nullptr,"PSMain","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&psBlob,&error)))return false;
    if(FAILED(device_->CreateVertexShader(vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),nullptr,&vs_)))return false; if(FAILED(device_->CreatePixelShader(psBlob->GetBufferPointer(),psBlob->GetBufferSize(),nullptr,&ps_)))return false;
    D3D11_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(Vertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(Vertex,tint),D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,0,offsetof(Vertex,light),D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(device_->CreateInputLayout(input,std::size(input),vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),&layout_)))return false;
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(MatrixConstants);cb.Usage=D3D11_USAGE_DYNAMIC;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;cb.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device_->CreateBuffer(&cb,nullptr,&matrixBuffer_)))return false;
    D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;if(FAILED(device_->CreateRasterizerState(&rs,&rasterizer_)))return false;
    D3D11_SAMPLER_DESC sm{};sm.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;sm.AddressU=sm.AddressV=sm.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sm.MaxLOD=D3D11_FLOAT32_MAX;return SUCCEEDED(device_->CreateSamplerState(&sm,&sampler_));
}
bool Renderer::CreateAtlas() {
    constexpr int width=80,height=16;std::vector<std::uint32_t> pixels(width*height);PaintTile(pixels,0,0xff4f9f35,0xff77bc45);PaintTile(pixels,1,0xff6f9140,0xff8a5b32);PaintTile(pixels,2,0xff795231,0xff8e6641);PaintTile(pixels,3,0xff777777,0xff969696);PaintTile(pixels,4,0xff272727,0xff4a4a4a);
    D3D11_TEXTURE2D_DESC td{};td.Width=width;td.Height=height;td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};ComPtr<ID3D11Texture2D> texture;if(FAILED(device_->CreateTexture2D(&td,&data,&texture)))return false;return SUCCEEDED(device_->CreateShaderResourceView(texture.Get(),nullptr,&atlas_));
}
bool Renderer::IsChunkVisible(ChunkCoord coord, DirectX::CXMMATRIX matrix) const {
    DirectX::XMFLOAT4X4 m;DirectX::XMStoreFloat4x4(&m,matrix);std::array<DirectX::XMFLOAT4,6> p={{{m._14+m._11,m._24+m._21,m._34+m._31,m._44+m._41},{m._14-m._11,m._24-m._21,m._34-m._31,m._44-m._41},{m._14+m._12,m._24+m._22,m._34+m._32,m._44+m._42},{m._14-m._12,m._24-m._22,m._34-m._32,m._44-m._42},{m._13,m._23,m._33,m._43},{m._14-m._13,m._24-m._23,m._34-m._33,m._44-m._43}}};
    const float minx=coord.x*ChunkSize,maxx=minx+ChunkSize,miny=0,maxy=ChunkHeight,minz=coord.z*ChunkSize,maxz=minz+ChunkSize;for(auto plane:p){float len=std::sqrt(plane.x*plane.x+plane.y*plane.y+plane.z*plane.z);if(len>0){plane.x/=len;plane.y/=len;plane.z/=len;plane.w/=len;}float x=plane.x>=0?maxx:minx,y=plane.y>=0?maxy:miny,z=plane.z>=0?maxz:minz;if(plane.x*x+plane.y*y+plane.z*z+plane.w<0)return false;}return true;
}
void Renderer::Render(const World& world,DirectX::XMFLOAT3 camera,DirectX::CXMMATRIX vp,int renderDistanceChunks) {
    const float clear[]={0.50f,0.72f,0.95f,1};context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),dsv_.Get());context_->ClearRenderTargetView(rtv_.Get(),clear);context_->ClearDepthStencilView(dsv_.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);D3D11_VIEWPORT viewport{0,0,static_cast<float>(width_),static_cast<float>(height_),0,1};context_->RSSetViewports(1,&viewport);context_->RSSetState(rasterizer_.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(context_->Map(matrixBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){MatrixConstants c{};DirectX::XMStoreFloat4x4(&c.viewProjection,vp);memcpy(mapped.pData,&c,sizeof(c));context_->Unmap(matrixBuffer_.Get(),0);}context_->IASetInputLayout(layout_.Get());context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context_->VSSetShader(vs_.Get(),nullptr,0);context_->VSSetConstantBuffers(0,1,matrixBuffer_.GetAddressOf());context_->PSSetShader(ps_.Get(),nullptr,0);context_->PSSetShaderResources(0,1,atlas_.GetAddressOf());context_->PSSetSamplers(0,1,sampler_.GetAddressOf());
    const int ccx=static_cast<int>(std::floor(camera.x/ChunkSize)),ccz=static_cast<int>(std::floor(camera.z/ChunkSize));
    for(const auto& [coord,chunk]:world.Chunks()){if(std::abs(coord.x-ccx)>renderDistanceChunks||std::abs(coord.z-ccz)>renderDistanceChunks||!IsChunkVisible(coord,vp)||chunk->Vertices().empty())continue;auto& gpu=gpuChunks_[coord];if(gpu.revision!=chunk->Revision()){D3D11_BUFFER_DESC bd{};bd.ByteWidth=static_cast<UINT>(chunk->Vertices().size()*sizeof(Vertex));bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA init{chunk->Vertices().data(),0,0};gpu.vertexBuffer.Reset();if(FAILED(device_->CreateBuffer(&bd,&init,&gpu.vertexBuffer)))continue;gpu.vertexCount=static_cast<UINT>(chunk->Vertices().size());gpu.revision=chunk->Revision();}UINT stride=sizeof(Vertex),offset=0;ID3D11Buffer* vb=gpu.vertexBuffer.Get();context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);context_->Draw(gpu.vertexCount,0);}swapChain_->Present(1,0);
}
} // namespace mc::game
