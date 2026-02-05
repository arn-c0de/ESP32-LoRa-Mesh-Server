#!/usr/bin/env bash
set -euo pipefail

# serial_monitor.sh - Open a serial monitor for the ESP32 (autodetects port)
# Usage: ./serial_monitor.sh [PORT] [-b BAUD]
# Defaults: BAUD=115200

BAUD=115200
PORT=""

# parse args: allow port or -b
while [[ $# -gt 0 ]]; do
  case "$1" in
    -b|--baud)
      BAUD="$2"; shift 2;;
    -h|--help)
      echo "Usage: $0 [PORT] [-b BAUD]"; exit 0;;
    *)
      if [[ -z "$PORT" ]]; then PORT="$1"; else echo "Unknown arg: $1"; exit 1; fi; shift;;
  esac
done

# If port not provided, collect all candidate ports (arduino-cli list + /dev/ttyUSB*/ttyACM*)
if [[ -z "$PORT" ]]; then
  candidates=()

  if command -v arduino-cli >/dev/null 2>&1; then
    # collect all ports shown by arduino-cli
    mapfile -t cli_ports < <(arduino-cli board list | awk 'NR>1 && $1!="" {print $1}') || true
    candidates+=("${cli_ports[@]:-}")
  fi

  # add /dev/ttyUSB* and /dev/ttyACM* devices
  for p in /dev/ttyUSB* /dev/ttyACM*; do
    if [[ -e "$p" ]]; then
      candidates+=("$p")
    fi
  done

  # deduplicate while preserving order
  declare -A _seen=()
  ports=()
  for p in "${candidates[@]:-}"; do
    if [[ -z "${_seen[$p]:-}" ]]; then
      _seen[$p]=1
      ports+=("$p")
    fi
  done

  if [[ ${#ports[@]} -eq 0 ]]; then
    echo "No serial port found. Connect device or specify port: $0 /dev/ttyUSB0" >&2
    exit 1
  elif [[ ${#ports[@]} -eq 1 ]]; then
    PORT="${ports[0]}"
  else
    echo "Multiple serial ports found:" 
    for i in "${!ports[@]}"; do
      idx=$((i+1))
      echo "  [$idx] ${ports[i]}"
    done

    # prompt for selection with default 1
    while true; do
      read -rp "Select port [1-${#ports[@]}] (default 1): " sel
      if [[ -z "$sel" ]]; then
        sel=1
        break
      elif [[ "$sel" =~ ^[0-9]+$ ]] && (( sel>=1 && sel<=${#ports[@]} )); then
        break
      else
        echo "Invalid selection: $sel" >&2
      fi
    done

    PORT="${ports[$((sel-1))]}"
  fi
fi

echo "Opening serial monitor on ${PORT} at ${BAUD} baud..."

# Prefer arduino-cli monitor if available
if command -v arduino-cli >/dev/null 2>&1; then
  exec arduino-cli monitor -p "${PORT}" -c baudrate=${BAUD}
fi

# Fallbacks: picocom -> screen
if command -v picocom >/dev/null 2>&1; then
  exec picocom -b ${BAUD} "${PORT}"
fi

if command -v screen >/dev/null 2>&1; then
  exec screen "${PORT}" ${BAUD}
fi

# Nothing available
echo "No serial monitor tool found. Install 'arduino-cli' or 'picocom' or use: screen ${PORT} ${BAUD}" >&2
exit 1
