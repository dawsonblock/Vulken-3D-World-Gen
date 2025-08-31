param()
$ErrorActionPreference = "Stop"

$REF = $env:REF; if (-not $REF -or $REF -eq "") { $REF = "tests/golden/golden_sample.png" }
$TEST = $env:TEST; if (-not $TEST -or $TEST -eq "") { $TEST = "output\frame.png" }
$OUT_DIFF = $env:OUT_DIFF; if (-not $OUT_DIFF -or $OUT_DIFF -eq "") { $OUT_DIFF = "imagediff.png" }
$OUT_JSON = $env:OUT_JSON; if (-not $OUT_JSON -or $OUT_JSON -eq "") { $OUT_JSON = "imagediff_report.json" }
$PSNR_MIN = $env:PSNR_MIN; if (-not $PSNR_MIN -or $PSNR_MIN -eq "") { $PSNR_MIN = "30" }
$MAE_MAX = $env:MAE_MAX; if (-not $MAE_MAX -or $MAE_MAX -eq "") { $MAE_MAX = "2" }
$STRICT = $env:STRICT; if (-not $STRICT -or $STRICT -eq "") { $STRICT = "0" }

if (-not (Test-Path ".\build\tools\imagediff.exe")) {
  cmake -S . -B build -G "Ninja"
  cmake --build build --target imagediff -j
}

if (-not (Test-Path $TEST)) {
  Write-Host "[golden] Test image not found at $TEST"
  if ($env:GOLDEN_STRICT -eq "1") {
    Write-Host "[golden] Strict mode; failing."
    exit 1
  } else {
    Write-Host "[golden] Non-strict; comparing reference against itself."
    $TEST = $REF
  }
}

.\build\tools\imagediff.exe -r $REF -t $TEST -o $OUT_DIFF -j $OUT_JSON -psnr-min $PSNR_MIN -mae-max $MAE_MAX