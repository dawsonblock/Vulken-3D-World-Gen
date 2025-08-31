
# Baseline & Pages

## Baseline regression gate
- Baseline file: `.bench/baseline.json`
- Primary metric: `mean_frametime_ms` (lower is better)
- CI env vars (Repository → Settings → Variables):
  - `TIDY_FAIL_ON_WARNINGS` = `true`/`false` (clang-tidy gate)
  - `PERF_REGRESSION_TOLERANCE` = `0.20` (20% by default)
  - `STRICT_BASELINE` = `true` to fail if baseline/current stat missing

Update `.bench/baseline.json` after you’re happy with a run (copy `mean_frametime_ms` from `bench_stats.json`).

## GitHub Pages
The `publish_pages` job publishes perf artifacts from `main` to Pages.
