# Golden Image Regression

This repo includes a tiny image-diff tool (`tools/imagediff`) and CI job `image_golden` to compare rendered frames to golden references.

## How it works
- Build `imagediff` (C++/stb) and compute MAE/MSE/PSNR between:
  - **Reference:** `tests/golden/reference.png`
  - **Test:** `output/frame.png` (or set `TEST=...`)
- Writes:
  - `imagediff_report.json` with metrics (`mae`, `mse`, `psnr`, `max_abs`, `pass`)
  - `imagediff.png` visualizing absolute per-channel differences
- CI thresholds (repo variables):
  - `GOLDEN_PSNR_MIN` (default 30 dB)
  - `GOLDEN_MAE_MAX` (default 2.0)
  - `GOLDEN_STRICT`   (0 = skip if missing test image, 1 = fail if missing)

## Local
```bash
# Build tools
cmake -S . -B build -G Ninja
cmake --build build --target imagediff -j

# Compare
REF=tests/golden/reference.png TEST=output/frame.png PSNR_MIN=30 MAE_MAX=2   ./scripts/golden_diff.sh
```

## Updating goldens
Replace or add files under `tests/golden/` and update CI `REF`/`TEST` envs as needed. Keep images small; large goldens bloat the repo and slow CI.
