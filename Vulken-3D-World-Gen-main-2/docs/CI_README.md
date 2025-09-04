# CI & Dev Environment Additions

This pack includes:
- **.github/workflows/ci.yml** — GitHub Actions for Ubuntu + Windows, GUI + headless.
- **vcpkg.json / vcpkg-configuration.json** — Lock and fetch GLFW/GLM/ImGui/STB/SPIRV-Tools.
- **CMakeUserPresets.json** — Local convenience presets using vcpkg toolchain.
- **Dockerfile** — Dev image with Vulkan SDK libs and vcpkg.
- **.devcontainer/** — VS Code Dev Container pre-wired to build headless by default.
- **.clang-format / .clang-tidy** — Basic style and static analysis.

## Local build (with vcpkg toolchain)
```bash
export VCPKG_ROOT=/path/to/vcpkg   # if not set by system or Dev Container
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release -j
```

## CI notes
- Ubuntu installs `libvulkan-dev`, validation layers, and shader tools.
- Windows uses Chocolatey to install the Vulkan SDK (sets `VULKAN_SDK` env).
- Matrix builds both `gui` and `headless` with CUDA/TensorRT disabled by default.
```


## New Jobs (Pro Pack)

- **Format Fix** (`.github/workflows/format_fix.yml`): Manual trigger to auto-apply clang-format and push.
- **Clang-Tidy** (`tidy` job in CI): Builds compile commands and runs clang-tidy; report artifact uploaded.
- **Perf Bench** (`perf_bench` job in CI): Runs headless for ~30s, parses logs, and uploads PNG histograms.

### Perf parsing notes
The parser looks for `FPS:` or `frametime: XX ms` patterns. If your app prints different labels, adjust regexes in
`.github/scripts/bench_parse.py` to match your logging format.


## New jobs
- **shader_check**: Compiles GLSL to SPIR-V on both Ubuntu/Windows using `glslc`. Uploads `.cache/spv` and caches by shader hash.
- **unit_tests**: Builds and runs GoogleTest-based unit tests on both OSes using vcpkg `gtest`.

### Local test run
```bash
# vcpkg toolchain recommended
cmake -S . -B build-tests -G Ninja -DENABLE_TESTS=ON
cmake --build build-tests --target voxelvk_tests -j
ctest --test-dir build-tests --output-on-failure
```

### Local shader compile
```bash
bash ./scripts/compile_shaders.sh
# Windows:
#   powershell -File .\scripts\compile_shaders.ps1
```
