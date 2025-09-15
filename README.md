# Vulken 3D World Gen

Minimal scaffold:
- C++: GLFW window + Vulkan instance/device init (prints selected GPU).
- Python: Perlin heightmap generator + viewer.

Prereqs (Ubuntu 24.04):
- System: cmake, build-essential, pkg-config, libglfw3-dev, libvulkan-dev, vulkan-validationlayers-dev (optional), python3, python3-venv, pip.
- Optional: shader tools (glslang-tools or shaderc) if you add shaders later.

Quick start:
- Setup deps:
  - ./scripts/setup-ubuntu.sh
- Build C++:
  - ./scripts/build.sh
- Run C++:
  - ./scripts/run.sh
  - (optional) enable validation: VULKAN_VALIDATION=1 ./scripts/run.sh
- Headless (no DISPLAY/WAYLAND): generates heightmap and saves viewer images
  - ./scripts/run.sh --headless
- Python (venv optional):
  - python3 -m venv .venv && source .venv/bin/activate
  - pip install -r python/requirements.txt
  - python python/worldgen/heightmap.py --out ./heightmap.png
  - python python/viewer.py --image ./heightmap.png
  - # 3D surface and histogram views:
  - python python/viewer.py --image ./heightmap.png --surface
  - python python/viewer.py --image ./heightmap.png --hist
  - # Save figures instead of showing:
  - MPLBACKEND=Agg python python/viewer.py --image ./heightmap.png --hist --save ./heightmap_view.png

Open Vulkan tutorial (optional):
- "$BROWSER" https://vulkan-tutorial.com/

Structure:
- src/: C++ sources
- scripts/: helper scripts
- python/: world-gen and viewer
- build/: cmake build output (gitignored)
