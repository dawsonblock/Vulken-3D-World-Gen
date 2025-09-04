# Vulken Engine Overview

Modules
- storage: Abstract I/O for local files (default), optional SQLite index, optional Redis backend.
- core: Shared config and voxel/mesh types.
- meshing: Marching Cubes / Dual Contouring implementations behind IMeshGenerator.
- render: IRenderer abstraction with VulkanRenderer default.
- VoxelChunkManager: Streaming, caching, and on-demand meshing.

Phases
- Phase 1: Engine skeleton (this), move demo logic into engine classes, add unit tests.
- Phase 2: Asset library and streaming (large .mvox, textures, SQLite index).
- Phase 3: Procedural/AI systems (noise, biomes, training hooks).
- Phase 4: Redis/network backend behind IStorageBackend.
- Phase 5: Tests + CI + packaging across platforms.
- Phase 6: ImGui Voxel Editor (place/paint/import/export).
- Phase 7: Multiplayer sync stub, AI-assisted editor, WASM demo.

Notes
- Keep interfaces stable; backends are swappable.
- Prioritize SSD-friendly chunk I/O and caching.
- Avoid coupling engine to apps; apps depend on engine.
