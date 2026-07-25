#include "game/Renderer.h"
#include "shared/LaunchGate.h"

#include <DirectXMath.h>
#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <string_view>

namespace {
mc::game::Renderer gRenderer;
int gWidth = 1280;
int gHeight = 720;
bool gRunning = true;

class ComScope {
public:
    ComScope() {
        result_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }

    ~ComScope() {
        if (SUCCEEDED(result_)) CoUninitialize();
    }

    bool Available() const {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }

private:
    HRESULT result_{E_FAIL};
};

LRESULT CALLBACK WindowProc(HWND window,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        gWidth = LOWORD(lParam);
        gHeight = HIWORD(lParam);
        gRenderer.Resize(gWidth, gHeight);
        return 0;
    case WM_DESTROY:
        gRunning = false;
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) DestroyWindow(window);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

std::filesystem::path ExecutablePath() {
    std::wstring path(32768, L'\0');
    const DWORD size = GetModuleFileNameW(
        nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size >= path.size()) return {};
    path.resize(size);
    return path;
}

bool TicketFromCommandLine(std::filesystem::path& ticket) {
    int argumentCount = 0;
    LPWSTR* arguments =
        CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) return false;

    bool found = false;
    for (int index = 1; index + 1 < argumentCount; ++index) {
        if (std::wstring_view(arguments[index]) == L"--launch-ticket") {
            ticket = arguments[index + 1];
            found = true;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}

bool KeyDown(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool PlayerCollides(const mc::game::World& world,
                    const DirectX::XMFLOAT3& feetPosition) {
    const DirectX::XMFLOAT3 minimum{
        feetPosition.x - mc::game::PlayerRadius,
        feetPosition.y,
        feetPosition.z - mc::game::PlayerRadius
    };
    const DirectX::XMFLOAT3 maximum{
        feetPosition.x + mc::game::PlayerRadius,
        feetPosition.y + mc::game::PlayerHeight,
        feetPosition.z + mc::game::PlayerRadius
    };
    return world.CollidesAabb(minimum, maximum);
}

bool MoveAxis(const mc::game::World& world,
              DirectX::XMFLOAT3& position,
              float distance,
              int axis) {
    if (std::abs(distance) < 1e-7f) return false;

    constexpr float maxStep = 0.02f;
    const int steps = std::max(
        1, static_cast<int>(std::ceil(std::abs(distance) / maxStep)));
    const float step = distance / static_cast<float>(steps);

    for (int index = 0; index < steps; ++index) {
        DirectX::XMFLOAT3 candidate = position;
        if (axis == 0) candidate.x += step;
        if (axis == 1) candidate.y += step;
        if (axis == 2) candidate.z += step;

        if (PlayerCollides(world, candidate)) return true;
        position = candidate;
    }
    return false;
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    std::filesystem::path ticket;
    std::wstring gateError;
    const auto executablePath = ExecutablePath();

    if (!TicketFromCommandLine(ticket) ||
        !mc::launchgate::ValidateAndConsumeLaunchTicket(
            ticket, executablePath, gateError)) {
        if (gateError.empty()) {
            gateError = L"Minecraft must be started by Launcher.exe.";
        }
        MessageBoxW(nullptr, gateError.c_str(),
                    L"Minecraft", MB_OK | MB_ICONERROR);
        return 2;
    }

    ComScope com;
    if (!com.Available()) {
        MessageBoxW(nullptr,
                    L"Windows imaging initialization failed.",
                    L"Minecraft", MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"MemphisCatMinecraftWindow";
    if (!RegisterClassExW(&windowClass)) return 1;

    RECT windowRectangle{0, 0, gWidth, gHeight};
    AdjustWindowRect(&windowRectangle, WS_OVERLAPPEDWINDOW, FALSE);

    HWND window = CreateWindowExW(
        0, windowClass.lpszClassName,
        L"Minecraft Direct3D 11",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRectangle.right - windowRectangle.left,
        windowRectangle.bottom - windowRectangle.top,
        nullptr, nullptr, instance, nullptr);

    const auto assetRoot =
        executablePath.parent_path() / L"assets";
    if (!window ||
        !gRenderer.Initialize(window, gWidth, gHeight, assetRoot)) {
        const std::wstring message =
            L"Direct3D 11 or texture initialization failed.\n\n"
            L"Expected assets at:\n" + assetRoot.wstring();
        MessageBoxW(window, message.c_str(),
                    L"Minecraft", MB_OK | MB_ICONERROR);
        return 1;
    }

    mc::game::World world;
    world.Generate(8);
    world.BuildAllMeshes();

    // Position is the center of the player's feet, not the camera.
    DirectX::XMFLOAT3 playerPosition{0.5f, 10.001f, -5.5f};
    DirectX::XMFLOAT3 velocity{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = -0.15f;
    bool onGround = false;
    bool wasJumpDown = false;
    bool wasLeftMouseDown = false;

    ShowCursor(FALSE);
    SetCapture(window);

    POINT center{};
    auto previousTime = std::chrono::steady_clock::now();

    while (gRunning) {
        MSG message{};
        while (PeekMessageW(
                &message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) gRunning = false;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!gRunning) break;

        const auto currentTime = std::chrono::steady_clock::now();
        float deltaTime =
            std::chrono::duration<float>(
                currentTime - previousTime).count();
        previousTime = currentTime;
        deltaTime = std::min(deltaTime, 0.05f);

        RECT client{};
        GetClientRect(window, &client);
        POINT topLeft{client.left, client.top};
        ClientToScreen(window, &topLeft);
        center = {
            topLeft.x + (client.right - client.left) / 2,
            topLeft.y + (client.bottom - client.top) / 2
        };

        if (GetForegroundWindow() == window) {
            POINT mouse{};
            GetCursorPos(&mouse);
            yaw += static_cast<float>(mouse.x - center.x) * 0.0025f;
            pitch += static_cast<float>(mouse.y - center.y) * 0.0025f;
            pitch = std::clamp(pitch, -1.5f, 1.5f);
            SetCursorPos(center.x, center.y);
        }

        const DirectX::XMFLOAT3 lookDirection{
            std::sin(yaw) * std::cos(pitch),
            -std::sin(pitch),
            std::cos(yaw) * std::cos(pitch)
        };
        const DirectX::XMFLOAT3 forward{
            std::sin(yaw), 0.0f, std::cos(yaw)
        };
        const DirectX::XMFLOAT3 right{
            forward.z, 0.0f, -forward.x
        };

        float inputForward = 0.0f;
        float inputRight = 0.0f;
        if (KeyDown('W')) inputForward += 1.0f;
        if (KeyDown('S')) inputForward -= 1.0f;
        if (KeyDown('D')) inputRight += 1.0f;
        if (KeyDown('A')) inputRight -= 1.0f;

        const float inputLength =
            std::sqrt(inputForward * inputForward +
                      inputRight * inputRight);
        if (inputLength > 0.0f) {
            inputForward /= inputLength;
            inputRight /= inputLength;
        }

        const float movementSpeed =
            KeyDown(VK_SHIFT)
                ? mc::game::SprintSpeed
                : mc::game::WalkSpeed;
        velocity.x =
            (forward.x * inputForward + right.x * inputRight) *
            movementSpeed;
        velocity.z =
            (forward.z * inputForward + right.z * inputRight) *
            movementSpeed;

        const bool jumpDown = KeyDown(VK_SPACE);
        if (jumpDown && !wasJumpDown && onGround) {
            velocity.y = mc::game::JumpSpeed;
            onGround = false;
        }
        wasJumpDown = jumpDown;

        velocity.y = std::max(
            velocity.y - mc::game::Gravity * deltaTime,
            -mc::game::TerminalVelocity);

        if (MoveAxis(world, playerPosition,
                     velocity.x * deltaTime, 0)) {
            velocity.x = 0.0f;
        }
        if (MoveAxis(world, playerPosition,
                     velocity.z * deltaTime, 2)) {
            velocity.z = 0.0f;
        }

        const bool verticalCollision =
            MoveAxis(world, playerPosition,
                     velocity.y * deltaTime, 1);
        if (verticalCollision) {
            onGround = velocity.y < 0.0f;
            velocity.y = 0.0f;
        } else {
            DirectX::XMFLOAT3 groundProbe = playerPosition;
            groundProbe.y -= 0.03f;
            onGround = PlayerCollides(world, groundProbe);
        }

        if (playerPosition.y < -10.0f) {
            playerPosition = {0.5f, 10.001f, -5.5f};
            velocity = {0.0f, 0.0f, 0.0f};
        }

        DirectX::XMFLOAT3 cameraPosition{
            playerPosition.x,
            playerPosition.y + mc::game::PlayerEyeHeight,
            playerPosition.z
        };

        const bool leftMouseDown = KeyDown(VK_LBUTTON);
        if (leftMouseDown && !wasLeftMouseDown) {
            const auto hit = world.Raycast(
                cameraPosition, lookDirection,
                mc::game::BreakReach);
            if (hit.hit) world.BreakBlock(hit.block);
        }
        wasLeftMouseDown = leftMouseDown;

        using namespace DirectX;
        const XMVECTOR eye = XMLoadFloat3(&cameraPosition);
        const XMVECTOR direction = XMLoadFloat3(&lookDirection);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMMATRIX view = XMMatrixLookToLH(eye, direction, up);
        const float aspect =
            gHeight > 0
                ? static_cast<float>(gWidth) /
                  static_cast<float>(gHeight)
                : 1.0f;
        const XMMATRIX projection =
            XMMatrixPerspectiveFovLH(
                XM_PIDIV4, aspect, 0.05f, 256.0f);

        gRenderer.Render(
            world, cameraPosition,
            view * projection, 8);
    }

    ReleaseCapture();
    ShowCursor(TRUE);
    return 0;
}
