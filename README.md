# MemphisCat Minecraft (Direct3D 11)

This repository contains two Windows executables:

- `Launcher.exe`: validates a signed GitHub manifest, checks the installed game hash, updates the source to the signed commit, builds only the game, and starts it with a one-time launch ticket.
- `Minecraft.exe`: the Direct3D 11 voxel game. It refuses a normal direct launch when the one-time launcher ticket is absent, expired, reused, or invalid.

## Build

Install Visual Studio with **Desktop development with C++**, the Windows SDK, Git, CMake, and Windows PowerShell. Run:

```bat
build_all.bat
```

The script builds both programs, copies them into one folder, packages the texture assets, removes temporary CMake build folders, creates a desktop shortcut named **Minecraft Launcher**, and attempts to record the local executable hash.

Outputs:

```text
bin\Launcher.exe
bin\Minecraft.exe
bin\local_manifest.json
bin\assets\textures\blocks\...
Desktop\Minecraft Launcher.lnk
```

The complete build output is stored in `logs\build.log`. Start the desktop shortcut or `bin\Launcher.exe`; do not start `Minecraft.exe` directly.

## Launcher/update layout

Although `Launcher.exe` is inside `bin`, the launcher resolves `bin\..` as the installation root:

```text
Minecraft\
├── bin\
│   ├── Launcher.exe
│   ├── Minecraft.exe
│   ├── local_manifest.json
│   └── assets\
├── source\                 updater checkout
├── update-build-game\      temporary updater build
└── logs\
    ├── build.log
    └── launcher.log
```

The launcher validates the signed online manifest, fetches the exact source commit when required, builds into an isolated updater build folder, copies `Minecraft.exe` and the assets into `bin`, writes the local executable hash, and starts the game with a short-lived one-time launch ticket. A named single-instance lock prevents overlapping launcher updates.

Every path decision, manifest check, Git command, build command, exit code, and fatal error is appended to `logs\launcher.log`. The launcher never rebuilds or replaces itself.

A hash embedded in a public client is not a true secret. The channel hash is only channel binding; trust comes from the asymmetric signature. Keep the private signing key outside this repository. The launcher-only gate is practical launch policy, not unbreakable DRM.

## World

```text
y = 9       grass block
y = 6..8    3 dirt blocks
y = 1..5    5 stone blocks
y = 0       unbreakable bedrock
```

Implemented: 16x32x16 chunks, exposed-face meshing, chunk-distance and camera-frustum culling, one GPU buffer per changed visible chunk, DDA raycasting, strict 4.5-block breaking reach, and remeshing only the edited chunk plus immediate neighbors.

## Texture assets and grass color

The renderer loads the real PNG files from `bin\assets\textures\blocks` through Windows Imaging Component instead of generating placeholder colors.

- Grass top: `grass_top.png`
- Grass sides: `grass_side.png`
- Dirt: `dirt.png`
- Bedrock: `bedrock.png`
- Grass climate map: `grass.png` (256x256)
- Stone: `stone.png` when present; a deterministic gray fallback is used until that file is added

Grass tint uses the requested Minecraft lookup:

```cpp
temperature = clamp(temperature, 0.0, 1.0);
humidity = clamp(humidity, 0.0, 1.0);
adjusted_humidity = humidity * temperature;
int pixelX = (int)((1.0 - temperature) * 255.0);
int pixelY = (int)((1.0 - adjusted_humidity) * 255.0);
```

The shader samples that exact pixel from `grass.png`. The whole grayscale grass top is tinted. On `grass_side.png`, only green-dominant grass pixels receive the climate tint; the dirt portion remains unchanged.

## Player movement

Noclip has been removed. The player now has a 0.6-block-wide, 1.8-block-tall collision box, a 1.62-block eye height, gravity, terminal velocity, grounded jumping, and axis-separated collision against solid blocks.

## Controls

Mouse look; W/A/S/D walk; Shift sprints; Space jumps; left click breaks a block within 4.5 blocks; Escape quits.

## Signing a release

Update `manifest.json`, then run:

```powershell
powershell -ExecutionPolicy Bypass -File tools\sign_manifest.ps1 -Manifest manifest.json -PrivateKey C:\secure\minecraft_manifest_private.pem
```

Commit only the signed manifest. Never commit the private key or channel secret.
