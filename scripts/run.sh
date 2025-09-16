#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN="$PROJECT_ROOT/build/bin/vulken_viewer"

HEADLESS=0
ALL=0
SAVE_VIEW=""
AUTO=0
AUTO_INTERVAL=2

# Parse args (supports --save-view <path> or --save-view=path)
SAVE_VIEW_NEXT=0
for arg in "$@"; do
  if [[ $SAVE_VIEW_NEXT -eq 1 ]]; then
    SAVE_VIEW="$arg"
    SAVE_VIEW_NEXT=0
    continue
  fi
  case "$arg" in
    --headless) HEADLESS=1 ;;
    --all) ALL=1 ;;
    --save-view) SAVE_VIEW_NEXT=1 ;;
    --save-view=*)
      SAVE_VIEW="${arg#--save-view=}"
      ;;
    --auto) AUTO=1 ;;
    --auto=*)
      AUTO=1
      AUTO_INTERVAL="${arg#--auto=}"
      ;;
  esac
done

# Default to headless if no display and neither --headless nor --auto were requested
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" && $HEADLESS -eq 0 && $AUTO -eq 0 ]]; then
  echo "No display detected; defaulting to headless mode. Use '--auto' for browser viewer or set DISPLAY/WAYLAND_DISPLAY."
  HEADLESS=1
fi

# Decide if we actually need to build the Vulkan viewer binary
NEED_BIN=1
if [[ $HEADLESS -eq 1 || -n "$SAVE_VIEW" || $AUTO -eq 1 ]]; then
  NEED_BIN=0
fi
if [[ $ALL -eq 1 ]]; then
  NEED_BIN=1
fi

if [[ $NEED_BIN -eq 1 && ! -x "$BIN" ]]; then
  echo "Binary not found, building..."

  # Clean build dir if cache pins a missing toolchain or mismatched generator
  if [[ -f "$PROJECT_ROOT/build/CMakeCache.txt" ]]; then
    cache_gen="$(awk -F= '/^CMAKE_GENERATOR:/{print $2; exit}' "$PROJECT_ROOT/build/CMakeCache.txt" || true)"
    cache_tc="$(awk -F= '/^CMAKE_TOOLCHAIN_FILE:FILEPATH=/{print $2; exit}' "$PROJECT_ROOT/build/CMakeCache.txt" || true)"
    if [[ -n "${cache_tc:-}" && ! -f "$cache_tc" ]]; then
      echo "Clearing build directory due to missing toolchain file ($cache_tc)"
      rm -rf "$PROJECT_ROOT/build"
    fi
    if [[ -n "${cache_gen:-}" && "$cache_gen" != "Ninja" && -x "$PROJECT_ROOT/scripts/build.sh" ]]; then
      echo "Clearing build directory due to generator mismatch ($cache_gen != Ninja)"
      rm -rf "$PROJECT_ROOT/build"
    fi
  fi

  use_presets=0
  if [[ -x "$PROJECT_ROOT/scripts/build.sh" ]]; then
    # Only use presets if Ninja is available and the expected toolchain file exists
    if command -v ninja >/dev/null 2>&1 && [[ -f "$PROJECT_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
      use_presets=1
    fi
  fi

  if [[ $use_presets -eq 1 ]]; then
    "$PROJECT_ROOT/scripts/build.sh"
  else
    desired_gen=""
    if command -v ninja >/dev/null 2>&1; then desired_gen="Ninja"; else desired_gen="Unix Makefiles"; fi

    if [[ -f "$PROJECT_ROOT/build/CMakeCache.txt" ]]; then
      cache_gen="$(awk -F= '/^CMAKE_GENERATOR:/{print $2; exit}' "$PROJECT_ROOT/build/CMakeCache.txt" || true)"
      cache_tc="$(awk -F= '/^CMAKE_TOOLCHAIN_FILE:FILEPATH=/{print $2; exit}' "$PROJECT_ROOT/build/CMakeCache.txt" || true)"
      if [[ -n "${cache_gen:-}" && "$cache_gen" != "$desired_gen" ]]; then
        echo "Clearing build directory due to generator mismatch ($cache_gen != $desired_gen)"
        rm -rf "$PROJECT_ROOT/build"
      elif [[ -n "${cache_tc:-}" && ! -f "$cache_tc" ]]; then
        echo "Clearing build directory due to missing toolchain file ($cache_tc)"
        rm -rf "$PROJECT_ROOT/build"
      fi
    fi

    cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/build" -G "$desired_gen"
    cmake --build "$PROJECT_ROOT/build" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
  fi
fi

# If we needed a binary, verify after build
if [[ $NEED_BIN -eq 1 && ! -x "$BIN" ]]; then
  echo "Error: build finished but binary missing: $BIN"
  echo "Tip: run with: bash scripts/run.sh --headless"
  exit 1
fi

if [[ -n "$SAVE_VIEW" ]]; then
  # Save viewer output (uses Agg backend) and exit
  OUT_IMG="$PROJECT_ROOT/heightmap.png"
  echo "Generating heightmap and saving viewer output to: $SAVE_VIEW"
  python3 "$PROJECT_ROOT/python/worldgen/heightmap.py" --out "$OUT_IMG"
  MPLBACKEND=Agg python3 "$PROJECT_ROOT/python/viewer.py" --image "$OUT_IMG" --hist --save "$SAVE_VIEW"
  echo "Saved:"
  echo "  $OUT_IMG"
  echo "  $SAVE_VIEW (and histogram alongside)"
  if [[ -n "${BROWSER:-}" ]]; then
    MAIN_ABS="$(realpath "$SAVE_VIEW")"
    "$BROWSER" "file://$MAIN_ABS" >/dev/null 2>&1 || true
    BASE="${SAVE_VIEW%.*}"; EXT="${SAVE_VIEW##*.}"; [[ "$EXT" == "$SAVE_VIEW" ]] && EXT="png"
    HIST_PATH="${BASE}_hist.${EXT}"
    if [[ -f "$HIST_PATH" ]]; then
      HIST_ABS="$(realpath "$HIST_PATH")"
      "$BROWSER" "file://$HIST_ABS" >/dev/null 2>&1 || true
    fi
  fi
  exit 0
fi

# Automated mode: regenerate on an interval, serve via http.server, and open in browser
if [[ $AUTO -eq 1 ]]; then
  OUT_DIR="$PROJECT_ROOT/out"
  mkdir -p "$OUT_DIR"
  INDEX_HTML="$OUT_DIR/index.html"
  cat > "$INDEX_HTML" <<EOF
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <meta http-equiv="refresh" content="$AUTO_INTERVAL"/>
  <title>Heightmap Viewer (auto-refresh)</title>
  <style>body{margin:0;background:#111;color:#eee;font-family:sans-serif} .wrap{padding:8px} img{max-width:100vw;height:auto;display:block}</style>
</head>
<body>
  <div class="wrap">
    <h3>Heightmap</h3>
    <img src="heightmap_view.png?ts=\$(Date.now())" alt="heightmap"/>
    <h3>Histogram</h3>
    <img src="heightmap_view_hist.png?ts=\$(Date.now())" alt="histogram"/>
  </div>
</body>
</html>
EOF
  echo "Starting static server on http://localhost:5173/"
  ( cd "$OUT_DIR" && python3 -m http.server 5173 ) >/dev/null 2>&1 &
  SRV_PID=$!
  trap 'kill "$SRV_PID" >/dev/null 2>&1 || true' EXIT INT TERM
  if [[ -n "${BROWSER:-}" ]]; then
    "$BROWSER" "http://localhost:5173/" >/dev/null 2>&1 || true
  fi

  OUT_IMG="$OUT_DIR/heightmap.png"
  VIEW_IMG="$OUT_DIR/heightmap_view.png"
  while true; do
    python3 "$PROJECT_ROOT/python/worldgen/heightmap.py" --out "$OUT_IMG" >/dev/null
    MPLBACKEND=Agg python3 "$PROJECT_ROOT/python/viewer.py" --image "$OUT_IMG" --hist --save "$VIEW_IMG" >/dev/null
    sleep "$AUTO_INTERVAL"
  done
fi

# Do not hard-fail on missing display; headless/auto below will handle it
if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
  :
fi

if [[ $HEADLESS -eq 1 ]]; then
  echo "Running headless fallback (Python heightmap + saved images)..."
  OUT_IMG="$PROJECT_ROOT/heightmap.png"
  VIEW_IMG="$PROJECT_ROOT/heightmap_view.png"
  python3 "$PROJECT_ROOT/python/worldgen/heightmap.py" --out "$OUT_IMG"
  MPLBACKEND=Agg python3 "$PROJECT_ROOT/python/viewer.py" --image "$OUT_IMG" --hist --save "$VIEW_IMG"
  echo "Saved:"
  echo "  $OUT_IMG"
  echo "  $VIEW_IMG (and histogram alongside)"
  if [[ -n "${BROWSER:-}" ]]; then
    MAIN_ABS="$(realpath "$VIEW_IMG")"
    "$BROWSER" "file://$MAIN_ABS" >/dev/null 2>&1 || true
    BASE="${VIEW_IMG%.*}"; EXT="${VIEW_IMG##*.}"; [[ "$EXT" == "$VIEW_IMG" ]] && EXT="png"
    HIST_PATH="${BASE}_hist.${EXT}"
    if [[ -f "$HIST_PATH" ]]; then
      HIST_ABS="$(realpath "$HIST_PATH")"
      "$BROWSER" "file://$HIST_ABS" >/dev/null 2>&1 || true
    fi
  fi
  exit 0
fi

if [[ $ALL -eq 1 ]]; then
  OUT_IMG="$PROJECT_ROOT/heightmap.png"
  if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    echo "No display detected; skipping Vulkan viewer. Running Python viewer only..."
    python3 "$PROJECT_ROOT/python/worldgen/heightmap.py" --out "$OUT_IMG"
    python3 "$PROJECT_ROOT/python/viewer.py" --image "$OUT_IMG" --hist
    exit 0
  fi
  echo "Launching Vulkan viewer in background..."
  "$BIN" &
  VK_PID=$!
  trap 'kill "$VK_PID" >/dev/null 2>&1 || true' EXIT INT TERM
  echo "Generating heightmap and opening Python viewer..."
  python3 "$PROJECT_ROOT/python/worldgen/heightmap.py" --out "$OUT_IMG"
  python3 "$PROJECT_ROOT/python/viewer.py" --image "$OUT_IMG" --hist
  echo "Waiting for Vulkan viewer to exit (pid=$VK_PID)..."
  wait "${VK_PID:-}"
  trap - EXIT INT TERM
  exit 0
fi

exec "$BIN"
