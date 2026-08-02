# CubicChunks3 Completion Workspace

This branch is a protected development workspace for finishing OpenCubicChunks/CubicChunks3 before a later Fabric/Minecraft 26.2 port.

- Upstream repository: `OpenCubicChunks/CubicChunks3`
- Upstream branch: `dev`
- Audited upstream commit: `e23e0e42b550ed0ba653b5460350193cd84403c0`
- Workspace branch: `agent/cubicchunks3-phase1`
- Target order: make upstream NeoForge 1.21.6 functional first, then stabilize it, then port the completed architecture to Fabric 26.2.

The repository could not be forked automatically through the available GitHub integration. Until a proper fork is created, this branch stores reviewed upstream-applicable patches and engineering notes. Your `main` branch remains untouched.

## Current patch

`patches/0001-implement-cube-emptiness.patch`

This replaces two active `CubeAccess` stubs that currently report every cube section and every Y range as non-empty. The patch includes unit coverage for empty cubes, occupied sections, ranges outside a cube, negative cube coordinates, and returning a section to air.

## Apply to an upstream checkout

```bash
git clone --recursive https://github.com/OpenCubicChunks/CubicChunks3.git
git -C CubicChunks3 checkout dev
git -C CubicChunks3 apply ../patches/0001-implement-cube-emptiness.patch
git -C CubicChunks3 diff --check
```

Then run:

```bash
cd CubicChunks3
./gradlew test
./gradlew check
```

The patch has been reviewed against the upstream source and vanilla `ChunkAccess.isYSpaceEmpty` behavior. A complete Gradle build still requires a normal internet-connected Java 21 development machine because this execution environment cannot clone Gradle dependencies.

See `docs/PHASE1_AUDIT.md` for the current subsystem order and blockers.
