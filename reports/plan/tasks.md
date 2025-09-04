# VULKEN-3D-WORLD-GEN PRODUCTION UPGRADE PLAN

## Executive Summary
This is a comprehensive 11-phase production upgrade plan to clean, optimize, and productionize the Vulken-3D-World-Gen engine. The project currently has significant duplication issues, missing compute shaders, incomplete tests, and needs full CI/CD, containerization, and K8s deployment capabilities.

## Current State Analysis
- **Duplicate structure**: Nested `Vulken-3D-World-Gen-main-2/` directory with 99% identical content
- **Archive clutter**: Multiple `.zip`, `.tar.Z` files, macOS metadata (`__MACOSX/`)  
- **Missing shaders**: Incomplete voxel mesher compute shaders
- **Test coverage**: Basic tests exist but need expansion and benchmarking
- **No deployment**: Missing Docker/Helm/K8s setup
- **No operator console**: Basic ImGui demo needs promotion

## Hard Constraints & Gates
- **Deterministic builds**: Pin all toolchain versions, no floating deps
- **Zero Vulkan validation errors**: Hard gate on all shader compilation  
- **Clean git status**: No dirty working tree after any phase
- **Surgical commits**: Clear commit messages, minimal diffs per phase
- **Performance gates**: ≤10% regression tolerance on benchmarks

---

## PHASE EXECUTION PLAN

### Phase 0: ✅ Repo Snapshot & Policy [COMPLETED]
**Status**: Completed - inventory created, branch established
- [x] Create branch `upgrade/prod-pass-01`
- [x] Generate inventory files in `reports/inventory/`
- [x] Lock vcpkg baseline for deterministic builds

### Phase 1: 🚀 Duplicate/Junk Purge [CRITICAL - IN PROGRESS]  
**Priority**: HIGHEST - blocks all other work
- [ ] Create `scripts/clean/dedupe_and_purge.py` script
- [ ] Remove duplicate `Vulken-3D-World-Gen-main-2/` tree (4.5MB duplicate)
- [ ] Remove archives: `voxel-models-main.zip` (4.5MB), `Unzipper-1.14.tar.Z`, `Unsure ply-1.1.tar.Z`
- [ ] Remove macOS artifacts: `__MACOSX/` directory (2.1MB)
- [ ] Remove temp files: `imgui.ini`, `screenshot.png`, `*.log`
- [ ] Generate `reports/cleanup/dedupe_report.json`
- [ ] Update `.gitignore` with comprehensive patterns
- **Commit**: `chore(clean): dedupe + purge artifacts, lock vcpkg baseline`
- **Gate**: `git status` clean, protected code intact

### Phase 2: 📁 Directory Canonicalization 
**Dependencies**: Phase 1 complete
- [ ] Create canonical directory structure with `scripts/dev/ensure_structure.py`
- [ ] Ensure required directories exist:
  - `assets/{textures,meshes_library,palettes,fonts}`
  - `config/{engine,renderer,weather,datasets}.yaml`
  - `reports/{snapshots,bench,shaders}`  
  - `helm/vulken-3d/{Chart.yaml,values.yaml,templates/}`
- **Commit**: `chore(structure): normalize canonical dirs and configs`

### Phase 3: 🔧 Vulkan/Shader Restoration & Validation [TECHNICAL]
**Priority**: HIGH - core engine functionality  
- [ ] Create missing compute shaders:
  - `shaders/core/voxel_mesher.comp`
  - `shaders/core/chunk_cull.comp` 
  - `shaders/core/greedy_mesh.comp` (optional)
- [ ] Add post-processing shaders: `taa.frag`, `ssao.frag`, `ssr.frag`, `bilateral_blur.frag`
- [ ] Create `cmake/shaders.cmake` for GLSL→SPIR-V compilation
- [ ] Create `scripts/shaders/validate_layout.py` using spirv-cross
- [ ] Add headless shader compilation test: `ctest -R shaders`
- **Commit**: `feat(shaders): compute mesher + layout validator + CI hook`
- **Gate**: Zero Vulkan validation errors, layout check JSON clean

### Phase 4: 💾 Data/Assets & Redis/Postgres Wiring
- [ ] Create seed assets in `assets/` directories
- [ ] Create `scripts/data/load_assets_to_redis.py --seed` 
- [ ] Add `config/datasets.yaml` for storage backend selection
- [ ] Optional: Add Postgres schema `sql/schema.sql` (gated by `-DWORLD_PG=ON`)
- **Commit**: `feat(data): seed assets + Redis loader + datasets.yaml`
- **Gate**: `apps/p1_memory_demo` loads from Redis, cold start <3s

### Phase 5: 🧪 Tests & Benchmarks
- [ ] Unit tests (gtest): voxel math, mesher core, device caps, persistence  
- [ ] Integration tests: render smoke, weather cycle
- [ ] Benchmark tests (Google Benchmark): mesher, chunk generation
- [ ] Generate `reports/bench/latest.json` with regression checking
- **Commit**: `test(core): unit/integration + benchmark gates`  
- **Gate**: `ctest` green, perf ≤10% drift

### Phase 6: ⚙️ CI/CD (GitHub Actions)
- [ ] `build.yml`: Matrix (ubuntu, windows), vcpkg cache, RelWithDebInfo
- [ ] `shader-validate.yml`: shaderc + spirv-cross compilation + layout validation
- [ ] `lint.yml`: clang-tidy, cmakelint, pre-commit
- [ ] `bench.yml`: Nightly benchmarks with trend analysis
- [ ] `docker.yml`: Build/push with SBOM and vulnerability scanning
- **Commit**: `ci: build, shader-validate, bench, lint, docker publish`
- **Gate**: Cold CI <15m, cached <7m, all workflows green

### Phase 7: 🐳 Docker & Helm (K8s Ready) [DEPLOYMENT]
**Priority**: HIGH - production readiness
- [ ] Multi-stage `Dockerfile`: build→slim runtime, non-root user
- [ ] `docker-compose.yml`: engine + redis + optional postgres
- [ ] Helm chart `helm/vulken-3d`: GPU toggles, env/secrets, readiness probes
- [ ] Add `/healthz` HTTP endpoint for liveness checks
- **Commit**: `feat(ops): Docker multi-stage + Helm chart with GPU opts`
- **Gate**: Container runs headless smoke, helm install returns 200 on `/healthz`

### Phase 8: 🖥️ Operator Console (ImGui++)
- [ ] Promote ImGui demo to full Operator Console
- [ ] Add panels: World, Renderer, Weather, Perf, Assets
- [ ] Hot-reload `/config/*.yaml` with diff display
- [ ] Export snapshots: `reports/snapshots/session_<timestamp>.yaml`
- [ ] Optional HTTP API: `/healthz`, `/caps`, `/config/reload`, `/weather/state`
- **Commit**: `feat(gui): operator console panels + hot reload + HTTP control`  
- **Gate**: No frame hitch >10ms on toggles, idle CPU <15%

### Phase 9: 🤖 AI Pipeline Stubs (Future-Ready)
- [ ] Create `docs/AI_PIPELINE.md` with architecture diagrams
- [ ] Add `src/ai/mesher_bridge.*` stubs (metrics only unless `-DAI_TRT=ON`)
- [ ] Create `scripts/ai/train_stub.py` for synthetic data generation
- **Commit**: `docs(ai): pipeline stubs + trainer mock + build flags`
- **Gate**: AI stubs compile, tests pass with TRT disabled

### Phase 10: 📚 Documentation & Guides  
- [ ] `docs/ARCHITECTURE.md`: device caps, memory, pipelines, shaders, world
- [ ] `docs/OPERATOR_GUIDE.md`: local/Docker/Helm setup, controls, troubleshooting
- [ ] `docs/TESTING.md`: test/bench procedures and thresholds
- [ ] Update `RUN.md`: quickstart with exact commands
- **Gate**: New developer can build & run in <15 minutes following docs

### Phase 11: 📦 Release Packaging & Tagging
- [ ] Create `scripts/dev/make_release_bundle.py`
- [ ] Generate `release/vulken3d_<os>_<arch>_<sha>.zip` 
- [ ] Create `RELEASE_NOTES.md` from git log
- [ ] Tag `v0.9.0-prodp1` with GPG signature
- **Commit**: `release: bundle + notes`
- **Gate**: Release ZIP runs out-of-box on target OS

---

## SUCCESS CRITERIA (Definition of Done)
All criteria must be met before completion:

- [ ] ✅ `cmake --build` succeeds on Linux + Windows
- [ ] ✅ `ctest` unit + integration tests green  
- [ ] ✅ `reports/shaders/layout_check.json` shows 0 mismatches
- [ ] ✅ Benchmark gate ≤10% drift with JSON artifacts
- [ ] ✅ Docker image builds and runs headless smoke app
- [ ] ✅ Helm chart installs on GPU node, `/healthz` returns 200
- [ ] ✅ Operator console functional with hot-reload and snapshots  
- [ ] ✅ Release ZIP created and runnable with provided configs
- [ ] ✅ Repository clean: no duplicate trees, deterministic baseline locked
- [ ] ✅ Documentation complete: new developer can run in <15 minutes

## Risk Mitigation
- **Phase 1 Critical**: All subsequent phases depend on successful deduplication
- **Shader Validation**: Use headless/SwiftShader fallback if no GPU available
- **Performance Regression**: Automated benchmark comparison with previous runs
- **Build Determinism**: Locked vcpkg baseline + pinned tool versions

## Timeline Estimate
- **Phases 1-2**: 2-4 hours (cleanup + structure)
- **Phase 3**: 4-6 hours (shader restoration + validation)  
- **Phases 4-6**: 6-8 hours (data + tests + CI)
- **Phases 7-8**: 4-6 hours (deployment + GUI)
- **Phases 9-11**: 2-4 hours (docs + release)
- **Total**: 18-28 hours for complete production upgrade

---

**Next Action**: Execute Phase 1 Duplicate/Junk Purge immediately.