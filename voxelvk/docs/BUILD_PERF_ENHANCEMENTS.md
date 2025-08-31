
# Build/Perf Enhancements

## Compile and Pipeline Caching
- **ccache** enabled via `CMAKE_CXX_COMPILER_LAUNCHER=ccache` with Actions cache on `~/.ccache`.
- **Vulkan pipeline cache** (experimental): CI uses `VK_PIPELINE_CACHE_PATH=${{ github.workspace }}/.cache/vk_pipeline_cache/voxelvk_pipelines.cache`
  - Wire your engine to read/write this path for `vkCreatePipelineCache` to persist between runs.

## Nsight Systems (self-hosted GPU)
- Enable by setting repository variable `ENABLE_NSIGHT=true`.
- Optional: set `NSYS_RUNNER` to your GPU runner label (default `self-hosted`).
- Your runner must have:
  - NVIDIA driver + CUDA stack
  - **Nsight Systems CLI** (`nsys`) in PATH
- CI step: runs `scripts/nsys_headless.sh 10`, uploads `nsys_report.qdrep` and `.sqlite`.

## PR Perf Comments
- On pull requests, CI posts a sticky comment with the perf regression summary (from `perf_compare_report.md`).
- Full histograms + stats are in the `perf-bench` artifact and on **GitHub Pages** (from `main`).

