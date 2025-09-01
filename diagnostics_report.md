
## Additions per request (specialization, barriers, graphics smoke)
- **shaders_vk/ai/blockize.comp.glsl** — Replaced incomplete GLSL with valid compute shader (Vulkan 1.2).
- **apps/smoke_headless.cpp** — Overwrote smoke_headless with Vulkan init.
- **apps/CMakeLists.txt** — apps CMake updated to link Vulkan and add graphics smoke.
- **apps/smoke_graphics_headless.cpp** — Added headless graphics smoke app.
- **CMakeLists.txt** — Top-level CMake: shader compiler fallback + tests registered.
- **src/ai/ai_gpu_blockize.cpp** — Replaced ai_gpu_blockize.cpp with explicit descriptor/pipeline + specialization.
- **src/vk/barrier_helpers.hpp** — Added barrier helper header.
