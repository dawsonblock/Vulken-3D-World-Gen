
# Integration Steps (Physics, Visuals, Persistence, IBL/CSM/PCSS)

## 1) Physics (choose one)
- **AABB controller**: `src/physics/player_controller.py` (swept axis, step-up, jump).
- **Capsule SAT controller**: `src/physics/player_controller_capsule.py` (smoother stairs).

Your engine must expose: `world.get_block_at_world_position(x,y,z) -> block_id`.
Return `0` (or your AIR id) for empty; non-zero for solid.

## 2) Visuals
- Add includes from `shaders/lighting/pbr_common.glsl`, `shaders/lighting/ibl.glsl`, and `shaders/lighting/pcss.glsl`.
- Use `shaders/post/atmosphere.frag` as background and `shaders/post/clouds.frag` composited after opaque.
- Wire a BRDF LUT + prefiltered env using your renderer; sample shaders under `shaders/ibl/*`.

## 3) Shadows (CSM + PCSS)
- Use `shaders/shadows/csm_common.glsl` UBO layout for cascade data.
- Render depth into a `sampler2DArray` (layers = cascade count).
- In your main pass, compute cascade index and filter via PCSS (see `shaders/lighting/pcss.glsl`).

## 4) Persistence
- Use `src/world/persistence.py` — async save with RLE + Zstd/LZ4; blocking load or queue with `src/utils/async_io.py`.
- Mark chunks `dirty=True` when modified; on unload call `store.save_async(chunk)` and on shutdown `store.wait_all()`.

## 5) Materials
- Example config under `assets/config/materials.json` (constant PBR). Map to your block/material IDs.

## 6) Tests
- `python tests/test_capsule_ground.py` to sanity-check capsule-ground resolution logic.

---

## C++/Vulkan mapping tips
- GLSL files compile with `glslc` to SPIR-V directly. Include paths must match your shader compiler.
- PCSS & CSM snippets are API-agnostic; supply a std140 UBO and a 2D-array depth texture.
- Physics code is reference Python — port the algorithms 1:1 into your C++ update loop if your engine is C++.
