#!/usr/bin/env bash
set -euo pipefail

# Start a virtual X server and expose it over a web-based VNC (noVNC),
# then run the GUI app inside it for a proper visual experience in headless envs.

PORT=${PORT:-6080}
DISPLAY_NUM=${DISPLAY_NUM:-99}
SCREEN_RES=${SCREEN_RES:-1280x720x24}
GUI_ARGS=${GUI_ARGS:-}

XVFB=":${DISPLAY_NUM}"

echo "[start_gui_web] Using DISPLAY=${XVFB}, noVNC on http://127.0.0.1:${PORT}/"

# Ensure required tools exist
for bin in Xvfb x11vnc websockify; do
  if ! command -v "$bin" >/dev/null 2>&1; then
    echo "[start_gui_web] Missing dependency: $bin. Please install x11vnc, websockify, and a window manager (e.g., fluxbox)." >&2
    exit 1
  fi
done

# Start Xvfb
if ! pgrep -f "Xvfb ${XVFB} " >/dev/null 2>&1; then
  echo "[start_gui_web] Starting Xvfb ${XVFB} ${SCREEN_RES}"
  Xvfb ${XVFB} -screen 0 ${SCREEN_RES} >/tmp/xvfb.log 2>&1 &
  sleep 0.5
fi

# Start a lightweight window manager if available
if command -v fluxbox >/dev/null 2>&1; then
  echo "[start_gui_web] Starting fluxbox window manager"
  DISPLAY=${XVFB} fluxbox >/tmp/fluxbox.log 2>&1 &
  sleep 0.5
fi

# Start VNC server bound to localhost
if ! pgrep -f "x11vnc .* -display ${XVFB}" >/dev/null 2>&1; then
  echo "[start_gui_web] Starting x11vnc on :${DISPLAY_NUM} -> 5900"
  x11vnc -display ${XVFB} -rfbport 5900 -localhost -nopw -forever -shared \
    -noxdamage -repeat -o /tmp/x11vnc.log  >/dev/null 2>&1 &
  sleep 0.5
fi

# Start noVNC (websockify) on ${PORT}
NOVNC_WEB=${NOVNC_WEB:-/usr/share/novnc}
if [ ! -d "$NOVNC_WEB" ]; then
  echo "[start_gui_web] noVNC web directory not found at $NOVNC_WEB" >&2
  echo "[start_gui_web] Install with: sudo apt-get install -y novnc websockify" >&2
  exit 1
fi

if ! pgrep -f "websockify ${PORT} .*localhost:5900" >/dev/null 2>&1; then
  echo "[start_gui_web] Starting websockify noVNC on port ${PORT}"
  websockify --web "$NOVNC_WEB" ${PORT} localhost:5900 >/tmp/websockify.log 2>&1 &
  sleep 0.5
fi

# Run the GUI application on the virtual display
export DISPLAY=${XVFB}
echo "[start_gui_web] Launching GUI app on DISPLAY=${DISPLAY}"
"$(cd "$(dirname "$0")" && pwd)/run_gui.sh"
