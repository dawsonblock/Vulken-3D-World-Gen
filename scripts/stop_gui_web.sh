#!/usr/bin/env bash
set -euo pipefail

DISPLAY_NUM=${DISPLAY_NUM:-99}
PORT=${PORT:-6080}

XVFB=":${DISPLAY_NUM}"

echo "[stop_gui_web] Stopping services for DISPLAY=${XVFB}, PORT=${PORT}"

# Best-effort kills; ignore errors if not running
pkill -f "websockify .* ${PORT} .*localhost:5900" 2>/dev/null || true
pkill -f "x11vnc .* -display ${XVFB}" 2>/dev/null || true
pkill -f "fluxbox" 2>/dev/null || true
pkill -f "Xvfb ${XVFB} " 2>/dev/null || true

echo "[stop_gui_web] Done"
