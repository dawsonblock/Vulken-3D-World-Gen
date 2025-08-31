#!/usr/bin/env bash
# Run headless app for N seconds (default 30), capture output for perf parsing.
set -euo pipefail

DUR=${1:-30}

if [ -x "./scripts/run_headless.sh" ]; then
  timeout "${DUR}s" ./scripts/run_headless.sh || true
elif [ -x "./build/apps/smoke_headless" ]; then
  timeout "${DUR}s" ./build/apps/smoke_headless || true
else
  echo "[bench] No headless runner found."
fi
