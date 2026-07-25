# MemphisCat Minecraft (Direct3D 11)

This repository contains two Windows executables:

- `Launcher.exe`: validates a signed GitHub manifest, checks the installed game hash, updates the source to the signed commit, builds only the game, and starts it with a one-time launch ticket.
- `Minecraft.exe`: the Direct3D 11 voxel prototype. It refuses a normal direct launch when the one-time launcher ticket is absent, expired, reused, or invalid.

## Build

Install Visual Studio 2022 with **Desktop development with C++**, the Windows SDK, Git, and CMake. Run `build_all.bat`.

Outputs:

```text
dist\Launcher.exe
dist\game\Minecraft.exe
```

Start `dist\Launcher.exe`. Do not start the game executable directly.

## Launcher/update design

The launcher silently downloads `manifest.json` from `main` over HTTPS. It validates the schema, channel-key hash, RSA-PSS/SHA-256 signature, minimum launcher version, local release fields, and local game SHA-256. When an update is needed, it fetches the exact signed commit, performs a forced detached checkout, cleans generated files, runs `build_game.bat`, hashes the resulting game, writes `game\local_manifest.json`, and launches it. It never rebuilds itself.

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
