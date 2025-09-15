#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$PROJECT_ROOT/build/bin/vulken_viewer"

HEADLESS=0
ALL=0
for arg in "$@"; do
  [[ "$arg" == "--headless" ]] && HEADLESS=1
  [[ "$arg" == "--all" ]] && ALL=1
done

if [[ ! -x "$BIN" ]]; then
  echo "Binary not found, building..."
  "$PROJECT_ROOT/scripts/build.sh"
fi

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
  if [[ $HEADLESS -eq 0 ]]; then
    echo "Error: No X11/Wayland display found (DISPLAY/WAYLAND_DISPLAY not set)."
    echo "Run from a desktop session, forward X/Wayland, or use: $0 --headless [--all]"
    exit 1
  fi
fi

if [[ $HEADLESS -eq 1 ]]; then
  echo "Running headless fallback (Python heightmap + saved images)..."
  OUT_IMG="$PROJECT_ROOT/heightmap.png"
  VIEW_IMG="$PROJECT_ROOT/heightmap_view.png"
  # Generate heightmap
  python3 "$PROJECT_ROOT/python/worldgen/heightmap.py" --out "$OUT_IMG"
  # Save viewer output (uses Agg backend)
  MPLBACKEND=Agg python3 "$PROJECT_ROOT/python/viewer.py" --image "$OUT_IMG" --hist --save "$VIEW_IMG"
  echo "Saved:"
  echo "  $OUT_IMG"
  echo "  $VIEW_IMG (and histogram alongside)"
  exit 0
fi

if [[ $ALL -eq 1 ]]; then
  OUT_IMG="$PROJECT_ROOT/heightmap.png"
  echo "Launching Vulkan viewer in background..."
  "$BIN" &
  VK_PID=$!
  trap 'kill "$VK_PID" >/dev/null 2>&1 || true' EXIT INT TERM
  echo "Generating heightmap and opening Python viewer..."
  python3 "$PROJECT_ROOT/python/worldgen/heightmap.py" --out "$OUT_IMG"
  python3 "$PROJECT_ROOT/python/viewer.py" --image "$OUT_IMG" --hist
  echo "Waiting for Vulkan viewer to exit (pid=$VK_PID)..."
  wait "$VK_PID"
  trap - EXIT INT TERM
  exit 0
fi

exec "$BIN"
