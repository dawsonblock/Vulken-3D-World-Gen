# Engine Tests

- Add a CTest-enabled target and a unit test framework (e.g., Catch2 or GoogleTest).
- Suggested initial tests:
  - VoxelChunkManager loads/saves chunks with FileStorageBackend.
  - MeshGenerator produces valid (non-crashing) mesh for trivial inputs.
  - LRUCache evicts correctly.

Temporary smoke test idea:
- Build a tiny console app that constructs EngineConfig, FileStorageBackend, MeshGenerator, VoxelChunkManager,
  loads chunk {0,0,0}, and requests its mesh. Validate counts and return 0 on success.
