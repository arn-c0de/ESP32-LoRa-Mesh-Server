#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"
REQ_FILE="$SCRIPT_DIR/requirements.txt"
PYTHON_BIN="python3"

if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  echo "[ERROR] python3 not found"
  exit 1
fi

if [ ! -d "$VENV_DIR" ]; then
  echo "[INFO] Creating virtual environment..."
  "$PYTHON_BIN" -m venv "$VENV_DIR"
fi

# shellcheck disable=SC1091
source "$VENV_DIR/bin/activate"

if [ ! -f "$REQ_FILE" ]; then
  echo "[ERROR] requirements.txt not found at $REQ_FILE"
  exit 1
fi

echo "[INFO] Installing dependencies..."
pip install -r "$REQ_FILE"

echo "[INFO] Starting HomeServer..."
exec python "$SCRIPT_DIR/main.py"
