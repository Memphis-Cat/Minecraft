# CubicChunks3 Phase 1 Audit

Audited upstream: `OpenCubicChunks/CubicChunks3@e23e0e42b550ed0ba653b5460350193cd84403c0`

## Confirmed baseline

- Minecraft: 1.21.6
- Loader: NeoForge 21.6.4-beta
- Java toolchain: 21
- Required submodule: `OpenCubicChunks/CubicChunksCore`, branch `dev`
- Active source: `src/`; `src_old/` is reference material and must not be compiled back into the mod wholesale.
- Upstream itself says the mod is not yet usable or functional.

## Confirmed build blocker

The active Gradle configuration requires Java 21, but the push, pull-request, and reusable test workflows install Java 17. The workflows also do not validate normal pushes to `dev`. Patch 0002 corrects this before engine changes are trusted.

## Confirmed active implementation gaps

### Cube access

`CubeAccess.isYSpaceEmpty` and `CubeAccess.isSectionEmpty` are hardcoded to `false`. This causes air-only cubes to be treated as occupied. Patch 0001 implements section-aware behavior and tests it.

### Heightmaps

Cube heightmap methods are unfinished. `getHeightmaps` and `getOrCreateHeightmapUnprimed` throw, `hasPrimedHeightmap` always returns false, and `getHeight` returns a fixed superflat value. `LevelCube` also skips heightmap initialization and updates.

### Lighting

The active `LevelCube.setBlockState` path updates section emptiness but leaves light-property changes as a TODO. Upstream issue 26 explicitly tracks functional lighting.

### World generation

Upstream issue 27 explicitly tracks functional world generation. Cube noise, carvers, aquifers, structures, surface rules, and vanilla spawn behavior must be tested as one pipeline rather than patched independently without integration tests.

### Integration tests

Upstream issue 24 lists untested or only unit-tested paths across `MinecraftServer`, `Level`, `ChunkMap`, `ChunkHolder`, `ServerChunkCache`, and client/server cube sources. A functional build without these tests would be too easy to corrupt worlds with.

## Coordinate limitation discovered

Current `CubePos` storage packs 21 bits per axis into one `long`. That is enough for the project's stated million-block-scale goal, but it is not a full signed 32-bit cube coordinate. Do not redesign this yet: first make the existing architecture load, save, light, generate, transmit, and render cubes correctly. After that baseline is stable, introduce a versioned wide-coordinate key and save/network format.

## Execution order

1. **Build and CI baseline**
   - Java 21 everywhere.
   - Recursive submodule checkout.
   - `build`, `test`, `check`, and dedicated-server startup validation.

2. **Cube lifecycle correctness**
   - Emptiness and section indexing.
   - Dirty/saved state.
   - Cube creation, retrieval, ticketing, load/unload, and cache behavior.
   - Save/reload round-trip tests.

3. **Networking**
   - Cube load/unload packets.
   - Block, block-entity, biome, and tick data.
   - Reconnect and dimension-change tests.

4. **Lighting**
   - Block light across cube boundaries.
   - Skylight with sparse cubes.
   - Empty-cube propagation and unload safety.

5. **World generation**
   - Preserve vanilla terrain at normal heights.
   - Empty buildable cubes above and below the vanilla generation band.
   - Structures, caves, aquifers, ores, surface rules, and spawn.

6. **Client rendering and interaction**
   - Vertical view distance and culling.
   - Mesh rebuilds and block updates.
   - Particles, sounds, selection, collision, and commands at large Y.

7. **Stabilization**
   - Crash-free client/server startup.
   - Save compatibility policy.
   - Performance and memory profiling.
   - Automated corruption and boundary tests.

8. **Wide coordinates**
   - Replace 21-bit packed cube keys with a versioned representation.
   - Migrate region storage, maps, tickets, packets, and debug tooling.
   - Target signed 32-bit block Y first; do not target signed 64-bit entity coordinates.

9. **Fabric 26.2 port**
   - Keep loader-neutral cube/core logic separate.
   - Replace NeoForge entrypoints, networking, events, config, and platform hooks with Fabric equivalents.
   - Port only after the NeoForge reference implementation is functionally complete.

## Validation rule

No subsystem is considered complete merely because it compiles. Each phase requires automated tests plus a real client/server smoke test and a save/reload test where applicable.
