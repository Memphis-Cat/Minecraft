# MemphisCat Minecraft (Direct3D 11)

This repository contains two Windows executables:

- `Launcher.exe`: validates a signed GitHub manifest, checks the installed game hash, updates the source to the signed commit, builds only the game, and starts it with a one-time launch ticket.
- `Minecraft.exe`: the Direct3D 11 voxel prototype. It refuses a normal direct launch when the one-time launcher ticket is absent, expired, reused, or invalid.

## Build

Install Visual Studio 2022 with **Desktop development with C++**, the Windows SDK, Git, CMake, and Windows PowerShell. Run:

```bat
build_all.bat
```

The script builds both programs, copies them into one folder, removes temporary CMake build folders, removes the legacy `dist` output, creates a desktop shortcut named **Minecraft Launcher**, and then attempts to record the local executable hash. Shortcut creation happens before online manifest stamping, so a temporary network problem cannot prevent the shortcut from being created.

Outputs:

```text
bin\Launcher.exe
bin\Minecraft.exe
bin\local_manifest.json
Desktop\Minecraft Launcher.lnk
```

The complete build output is stored in:

```text
logs\build.log
```

Start the desktop shortcut or `bin\Launcher.exe`. Do not start `Minecraft.exe` directly.

## Launcher/update layout

Although `Launcher.exe` is inside `bin`, the launcher first resolves `bin\..` as the installation root. Runtime files are separated like this:

```text
Minecraft\
├── bin\
│   ├── Launcher.exe
│   ├── Minecraft.exe
│   └── local_manifest.json
├── source\                 updater checkout
└── logs\
    ├── build.log
    └── launcher.log
```

Older builds incorrectly created `bin\source`. The corrected build and launcher remove that generated legacy folder.

The launcher silently downloads `manifest.json` from `main` over HTTPS. It validates the schema, channel-key hash, RSA-PSS/SHA-256 signature, minimum launcher version, local release fields, and local game SHA-256.

When an update is needed, the launcher:

1. Changes its logical root from `bin` to the parent installation folder.
2. Fetches the exact signed source commit into `source` beside `bin`.
3. Performs a forced clean checkout.
4. Runs that source revision's `build_game.bat`.
5. Places the rebuilt game at `bin\Minecraft.exe`.
6. Removes the temporary `build-game` folder.
7. Hashes the resulting executable and writes `bin\local_manifest.json`.
8. Starts the game with a short-lived one-time launch ticket.

Every path decision, manifest check, Git command, build command, command exit code, and fatal error is appended to:

```text
logs\launcher.log
```

Launcher error dialogs include the exact log path. It never rebuilds or replaces `Launcher.exe`. A launcher update must be built manually with `build_all.bat` or `build_launcher.bat`.

A hash embedded in a public client is not a true secret. The channel hash is only channel binding; trust comes from the asymmetric signature. Keep the private signing key outside this repository. The launcher-only gate is practical launch policy, not unbreakable DRM: it uses a per-user DPAPI-protected install secret and a short-lived one-time HMAC ticket.

## Voxel prototype

World layers:

```text
y = 9       grass block
y = 6..8    3 dirt blocks
y = 1..5    5 stone blocks
y = 0       unbreakable bedrock
```

Implemented: 16x32x16 chunks, exposed-face meshing, chunk-distance and camera-frustum culling, one GPU buffer per changed visible chunk, grass-top/grass-side/dirt/stone/bedrock atlas tiles, requested temperature/humidity tint formula, DDA raycast, strict 4.5-block breaking reach, and remeshing only the edited chunk plus immediate neighbors.

The first game milestone uses noclip movement so the launcher, update integrity, chunk system, culling, meshing, and raycast can be tested before collision, persistence, terrain noise, greedy meshing, occlusion queries, and multithreaded streaming.

## Controls

Mouse look; W/A/S/D move; Space/Ctrl move vertically; Shift moves faster; left click breaks within 4.5 blocks; Escape quits.

## Signing a release

Update `manifest.json`, then run:

```powershell
powershell -ExecutionPolicy Bypass -File tools\sign_manifest.ps1 -Manifest manifest.json -PrivateKey C:\secure\minecraft_manifest_private.pem
```

Commit only the signed manifest. Never commit the private key or channel secret.
