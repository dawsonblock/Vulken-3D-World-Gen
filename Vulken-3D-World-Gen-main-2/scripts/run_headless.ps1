# Windows helper to run headless binary with defaults
$ErrorActionPreference = "Stop"

# Default Vulkan pipeline cache path if not provided
if (-not $env:VK_PIPELINE_CACHE_PATH -or $env:VK_PIPELINE_CACHE_PATH -eq "") {
  $cacheDir = Join-Path (Split-Path $PSScriptRoot -Parent) ".cache\vk_pipeline_cache"
  if (-not (Test-Path $cacheDir)) { New-Item -ItemType Directory -Path $cacheDir | Out-Null }
  $env:VK_PIPELINE_CACHE_PATH = Join-Path $cacheDir "voxelvk_pipelines.cache"
}

$exe = "build\apps\smoke_headless.exe"
if (-not (Test-Path $exe)) {
  $exe = "build\apps\smoke_graphics_headless.exe"
}

if (Test-Path $exe) {
  Write-Host "[run_headless.ps1] Using VK_PIPELINE_CACHE_PATH=$($env:VK_PIPELINE_CACHE_PATH)"
  & $exe
} else {
  Write-Host "[run_headless.ps1] Executable not found."
  exit 1
}
