#include "game/Renderer.h"
#include "shared/LaunchGate.h"

#include <windows.h>
#include <shellapi.h>
#include <DirectXMath.h>
#include <chrono>
#include <filesystem>
#include <string>

namespace {
mc::game::Renderer gRenderer; int gWidth=1280,gHeight=720; bool gRunning=true;
LRESULT CALLBACK WindowProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){switch(msg){case WM_SIZE:gWidth=LOWORD(l);gHeight=HIWORD(l);gRenderer.Resize(gWidth,gHeight);return 0;case WM_DESTROY:gRunning=false;PostQuitMessage(0);return 0;case WM_KEYDOWN:if(w==VK_ESCAPE)DestroyWindow(hwnd);return 0;}return DefWindowProcW(hwnd,msg,w,l);}
std::filesystem::path ExecutablePath(){std::wstring p(32768,L'\0');DWORD n=GetModuleFileNameW(nullptr,p.data(),static_cast<DWORD>(p.size()));p.resize(n);return p;}
bool TicketFromCommandLine(std::filesystem::path& ticket){int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)return false;bool found=false;for(int i=1;i+1<argc;++i)if(std::wstring_view(argv[i])==L"--launch-ticket"){ticket=argv[i+1];found=true;break;}LocalFree(argv);return found;}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
    std::filesystem::path ticket;std::wstring gateError;if(!TicketFromCommandLine(ticket)||!mc::launchgate::ValidateAndConsumeLaunchTicket(ticket,ExecutablePath(),gateError)){if(gateError.empty())gateError=L"Minecraft must be started by Launcher.exe.";MessageBoxW(nullptr,gateError.c_str(),L"Minecraft",MB_OK|MB_ICONERROR);return 2;}
    WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=WindowProc;wc.hInstance=instance;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.lpszClassName=L"MemphisCatMinecraftWindow";if(!RegisterClassExW(&wc))return 1;RECT r{0,0,gWidth,gHeight};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"Minecraft Direct3D 11 - voxel bootstrap",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!hwnd||!gRenderer.Initialize(hwnd,gWidth,gHeight)){MessageBoxW(hwnd,L"Direct3D 11 initialization failed.",L"Minecraft",MB_OK|MB_ICONERROR);return 1;}
    mc::game::World world;world.Generate(8);world.BuildAllMeshes();DirectX::XMFLOAT3 position{0.5f,12.0f,-5.0f};float yaw=0,pitch=-0.15f;ShowCursor(FALSE);SetCapture(hwnd);POINT center{};auto last=std::chrono::steady_clock::now();bool wasLeft=false;
    while(gRunning){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)gRunning=false;TranslateMessage(&msg);DispatchMessageW(&msg);}if(!gRunning)break;auto now=std::chrono::steady_clock::now();float dt=std::chrono::duration<float>(now-last).count();last=now;dt=(dt>0.05f)?0.05f:dt;
        RECT client{};GetClientRect(hwnd,&client);POINT tl{client.left,client.top};ClientToScreen(hwnd,&tl);center={tl.x+(client.right-client.left)/2,tl.y+(client.bottom-client.top)/2};POINT mouse{};GetCursorPos(&mouse);if(GetForegroundWindow()==hwnd){yaw+=(mouse.x-center.x)*0.0025f;pitch+=(mouse.y-center.y)*0.0025f;pitch=(pitch<-1.5f)?-1.5f:(pitch>1.5f?1.5f:pitch);SetCursorPos(center.x,center.y);} 
        DirectX::XMFLOAT3 forward{std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch)};DirectX::XMFLOAT3 flat{std::sin(yaw),0,std::cos(yaw)},right{flat.z,0,-flat.x};float speed=(GetAsyncKeyState(VK_SHIFT)&0x8000)?14.0f:7.0f;auto key=[](int k){return (GetAsyncKeyState(k)&0x8000)!=0;};if(key('W')){position.x+=flat.x*speed*dt;position.z+=flat.z*speed*dt;}if(key('S')){position.x-=flat.x*speed*dt;position.z-=flat.z*speed*dt;}if(key('D')){position.x+=right.x*speed*dt;position.z+=right.z*speed*dt;}if(key('A')){position.x-=right.x*speed*dt;position.z-=right.z*speed*dt;}if(key(VK_SPACE))position.y+=speed*dt;if(key(VK_CONTROL))position.y-=speed*dt;
        bool left=key(VK_LBUTTON);if(left&&!wasLeft){auto hit=world.Raycast(position,forward,mc::game::BreakReach);if(hit.hit)world.BreakBlock(hit.block);}wasLeft=left;
        using namespace DirectX;XMVECTOR eye=XMLoadFloat3(&position),dir=XMLoadFloat3(&forward),up=XMVectorSet(0,1,0,0);XMMATRIX view=XMMatrixLookToLH(eye,dir,up);float aspect=gHeight?static_cast<float>(gWidth)/gHeight:1.0f;XMMATRIX projection=XMMatrixPerspectiveFovLH(XM_PIDIV4,aspect,0.05f,256.0f);gRenderer.Render(world,position,view*projection,8);
    }ReleaseCapture();ShowCursor(TRUE);return 0;
}
