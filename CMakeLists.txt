cmake_minimum_required(VERSION 3.24)
project(MemphisCatMinecraft LANGUAGES CXX)

option(MC_BUILD_LAUNCHER "Build the launcher" ON)
option(MC_BUILD_GAME "Build the game" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin/${config}")
endforeach()

function(mc_windows_target target)
    target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
        target_link_options(${target} PRIVATE /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA)
    endif()
endfunction()

add_library(mc_shared STATIC
    shared/Crypto.cpp
    shared/LaunchGate.cpp
)
target_include_directories(mc_shared PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(mc_shared PUBLIC bcrypt crypt32 shell32)
mc_windows_target(mc_shared)

if(MC_BUILD_LAUNCHER)
    add_executable(Launcher WIN32
        launcher/main.cpp
        launcher/Manifest.cpp
        launcher/Updater.cpp
    )
    target_link_libraries(Launcher PRIVATE mc_shared winhttp shlwapi)
    mc_windows_target(Launcher)
endif()

if(MC_BUILD_GAME)
    add_executable(Minecraft WIN32
        game/main.cpp
        game/World.cpp
        game/Renderer.cpp
    )
    target_link_libraries(Minecraft PRIVATE mc_shared d3d11 dxgi d3dcompiler)
    mc_windows_target(Minecraft)
endif()
