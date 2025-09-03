# Engine Architecture (Phase 1 Scaffolding)

- Modules
  - Storage: IStorageBackend, LocalFileStorageBackend (file-based, optional SQLite index in Phase 2/4).
  - Cache: IChunkCache, SimpleLRUCache (thread-unsafe, replace with lock-free/segmented in Phase 5).
  - Core: Voxel types, ChunkCoord, VoxelChunkManager (I/O + cache orchestration).
  - Mesh: IMeshGenerator, MarchingCubesGenerator (stub; dual contouring later).
  - Renderer: VulkanRenderer facade (decoupled from demo apps; real wiring later).

- Data Flow
  ProcGen → VoxelChunk → MeshGenerator → Mesh → Renderer

- Extensibility
  - Swap storage backends (local, SQLite, Redis).
  - Swap mesh generators (MC, DC).
  - Plug into existing demos by including engine headers and linking VulkenEngine.

- Next Steps
  - Integrate CMake (add_subdirectory(src/engine)).
  - Migrate demo logic to use VoxelChunkManager and MeshGenerator interfaces.
  - Add unit tests and golden image tests (Phase 5).
