# Release: Modern GUI Milestone (2025-09-02)

Highlights
- Full ImGui + Vulkan integration (GLFW + Vulkan backends) with modern UX: docking, optional multi-viewports, themes, status bar, and Ctrl+K command palette.
- Robust Vulkan presentation path: swapchain, render pass, framebuffers, command buffers, and sync wired for real UI rendering.
- Headless GUI smoke via Xvfb with autoscreenshot and JSON performance export (GPU timings for RenderPass and ImGui).
- Performance instrumentation (CPU/GPU) with JSON export and optional CI gating hooks.
- CI workflows for build/test and GUI smoke; artifacts include screenshot.png and performance_report.json.
- Pipeline cache persistence (opt-in via env), cleaner CLI/env handling for UI scale and ini, unified fullscreen toggle.
- Warning cleanups and safer Vulkan struct initialization.

How to run (local)
- Build GUI app
  - cmake --build build --target main_imgui_vulkan --parallel
- Headless smoke (produces screenshot + perf JSON)
  - DISPLAY= xvfb-run -a -s "-screen 0 1280x720x24" ./build/main_imgui_vulkan --smoke --autoscreenshot --benchmark --frames 10 --export-perf --perf-out .

CI gating
- To enforce GUI smoke performance gate on PRs, set Actions variable GUI_SMOKE_STRICT=true in the repo settings.
  - GitHub UI: Settings → Secrets and variables → Actions → Variables → New variable → Name: GUI_SMOKE_STRICT, Value: true.

Artifacts
- screenshot.png
- performance_report.json (includes gpu_timings for "ImGui" and "RenderPass")

Notes
- Optional multi-viewports can be enabled via --viewports or VOXELVK_VIEWPORTS=1.
- UI scale can be adjusted via --ui-scale or VOXELVK_UI_SCALE.
