#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

have() { command -v "$1" >/dev/null 2>&1; }
log() { echo "[start-demo] $*"; }

open_when_ready() {
  local url="$1"
  if [ -z "${BROWSER:-}" ]; then
    log "BROWSER not set; open manually: $url"
    return 0
  fi
  (
    for _ in $(seq 1 60); do
      if curl -fsS "$url" >/dev/null 2>&1; then
        log "Opening $url"
        "$BROWSER" "$url" >/dev/null 2>&1 || true
        exit 0
      fi
      sleep 0.5
    done
    log "Service not reachable yet: $url"
  ) &
}

start_node() {
  log "Detected Node.js project"
  if ! have npm; then log "npm not found"; return 1; fi

  if [ -f package-lock.json ]; then
    log "Installing deps (npm ci)"
    npm ci
  else
    log "Installing deps (npm install)"
    npm install
  fi

  # Open likely URLs based on common frameworks
  grep -q '"vite"' package.json 2>/dev/null && open_when_ready "http://localhost:5173"
  grep -q '"next"' package.json 2>/dev/null && open_when_ready "http://localhost:3000"
  grep -q '"react-scripts"' package.json 2>/dev/null && open_when_ready "http://localhost:3000"

  if grep -q '"dev"\s*:' package.json; then
    log "Running: npm run dev"
    exec npm run dev
  elif grep -q '"start"\s*:' package.json; then
    log "Running: npm start"
    exec npm start
  elif grep -q '"vite"' package.json; then
    log "Running: npx vite"
    exec npx vite
  elif grep -q '"next"' package.json; then
    log "Running: npx next dev"
    exec npx next dev
  else
    log "No known start script. Try: npm run dev"
    exit 1
  fi
}

start_rust() {
  log "Detected Rust project"
  if ! have cargo; then log "cargo not found"; return 1; fi
  log "Building and running (cargo run --release)"
  exec cargo run --release
}

start_cmake() {
  log "Detected CMake/C++ project"
  have cmake || { log "cmake not found"; return 1; }
  mkdir -p build
  cmake -S . -B build
  cmake --build build -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
  local bin
  bin="$(find build -type f -perm -111 -exec file {} \; | grep -E 'ELF .* executable' | cut -d: -f1 | head -n1 || true)"
  if [ -n "$bin" ]; then
    log "Running: $bin"
    exec "$bin"
  else
    log "No executable found in build/"
    exit 1
  fi
}

start_python() {
  log "Detected Python project"
  have python3 || { log "python3 not found"; return 1; }
  if [ ! -d .venv ]; then python3 -m venv .venv; fi
  # shellcheck disable=SC1091
  source .venv/bin/activate
  python -m pip install -U pip >/dev/null
  [ -f requirements.txt ] && pip install -r requirements.txt
  if [ -f pyproject.toml ]; then
    pip install .
  fi

  if grep -qiE 'fastapi|uvicorn' requirements.txt 2>/dev/null || grep -qiE 'fastapi|uvicorn' pyproject.toml 2>/dev/null; then
    open_when_ready "http://localhost:8000"
    log "Running: uvicorn main:app --reload --host 0.0.0.0 --port 8000"
    exec uvicorn main:app --reload --host 0.0.0.0 --port 8000
  elif [ -f main.py ]; then
    log "Running: python main.py"
    exec python main.py
  elif [ -f app.py ]; then
    log "Running: python app.py"
    exec python app.py
  else
    log "No runnable entry point found (main.py/app.py)."
    exit 1
  fi
}

start_static() {
  log "Detected static web (index.html)"
  open_when_ready "http://localhost:5173"
  log "Serving: python3 -m http.server 5173"
  exec python3 -m http.server 5173
}

main() {
  if [ -f package.json ]; then
    start_node
  elif [ -f Cargo.toml ]; then
    start_rust
  elif [ -f CMakeLists.txt ]; then
    start_cmake
  elif [ -f requirements.txt ] || [ -f pyproject.toml ]; then
    start_python
  elif [ -f index.html ]; then
    start_static
  else
    log "Cannot detect project type."
    log "Add one of: package.json, Cargo.toml, CMakeLists.txt, requirements.txt/pyproject.toml, or index.html"
    exit 1
  fi
}

main "$@"
