#!/usr/bin/env bash
# Run headless binary under Nsight Systems for N seconds and save report.
set -euo pipefail
DUR=${1:-10}

EXE="./build/apps/smoke_headless"
if [ ! -x "$EXE" ] && [ -x "./scripts/run_headless.sh" ]; then
  # Attempt to run script once to build shaders/pipeline cache
  timeout 5s ./scripts/run_headless.sh || true
fi

if [ ! -x "$EXE" ]; then
  echo "Headless exe not found at $EXE"
  exit 1
fi

# If timeout is available, use it to limit run duration.
if command -v timeout >/dev/null 2>&1; then
  timeout "${DUR}s" nsys profile -o nsys_report --sample=none --trace=cuda,osrt,nvtx "$EXE" || true
else
  nsys profile -o nsys_report --sample=none --trace=cuda,osrt,nvtx "$EXE" &
  PID=$!
  sleep "$DUR"
  kill $PID || true
fi
