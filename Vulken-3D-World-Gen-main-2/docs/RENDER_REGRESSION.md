# Render Regression (Image Diff)

This framework compares produced PNG frames in `output/renders/` against goldens in `tests/golden/`:

- Script `scripts/render_frames.sh` runs your headless app briefly; if it doesn't dump frames yet, it copies
  `tests/golden/golden_sample.png` into `output/renders/golden_sample.png` as a placeholder so the pipeline still runs.
- Comparator `.github/scripts/image_diff.py` computes **SSIM** and **PSNR**, emits per-image diffs, and writes:
  - `image_diffs/image_diff_report.md`
  - `image_diffs/image_diff_results.json`

## CI thresholds
Configure in **Repo → Settings → Variables**:
- `STRICT_RENDER_TESTS` = `true` to fail the job on any regression or missing actuals.
- `DIFF_SSIM_MIN` (default `0.995`)
- `DIFF_PSNR_MIN` (default `35.0`)

## Integrating your renderer
Make your headless/GUI app write PNGs to `output/renders/`:
- Filenames should match goldens in `tests/golden/` (e.g. `golden_sample.png` → `output/renders/golden_sample.png`).
- Add more goldens to `tests/golden/` and commit.

## Local run
```bash
# Build headless + render (or placeholder)
bash scripts/render_frames.sh

# Diff locally
python3 .github/scripts/image_diff.py
# Results in image_diffs/
```
