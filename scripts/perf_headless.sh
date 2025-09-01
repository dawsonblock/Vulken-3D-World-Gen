#!/usr/bin/env bash
# Sample CPU profile using 'perf' if available.
set -euo pipefail
DUR=${1:-10}

EXE="./build/apps/smoke_headless"
if [ ! -x "$EXE" ]; then
  EXE="./build/apps/smoke_graphics_headless"
fi

if ! command -v perf >/dev/null 2>&1; then
  echo "[perf] 'perf' not available"
  exit 0
fi

echo "[perf] Recording for ${DUR}s..."
if command -v timeout >/dev/null 2>&1; then
  timeout "${DUR}s" perf record -F 99 -g -- "$EXE" || true
else
  perf record -F 99 -g -- "$EXE" &
  PID=$!
  sleep "$DUR"
  kill $PID || true
fi

echo "[perf] Generating report..."
perf report --stdio > perf_report.txt || true
echo "[perf] Done."
