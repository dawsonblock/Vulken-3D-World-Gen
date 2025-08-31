\
# Compile all GLSL shaders into SPIR-V into .cache\spv using glslc from the Vulkan SDK.
$ErrorActionPreference = "Stop"

$outDir = $env:OUT_DIR
if (-not $outDir -or $outDir -eq "") { $outDir = ".cache\spv" }
$glslc = $env:GLSLC
if (-not $glslc -or $glslc -eq "") { $glslc = "glslc.exe" }

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$paths = @("shaders","src\shaders","assets\shaders")
$found = 0
foreach ($p in $paths) {
  if (Test-Path $p) {
    Get-ChildItem -Path $p -Recurse -Include *.vert,*.frag,*.comp,*.glsl | ForEach-Object {
      $base = $_.Name + ".spv"
      $out = Join-Path $outDir $base
      Write-Host "[glslc] $($_.FullName) -> $out"
      & $glslc -O $_.FullName -o $out
      $found += 1
    }
  }
}
Write-Host "[glslc] compiled: $found file(s)"
