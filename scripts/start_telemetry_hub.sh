#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
device="${1:-/dev/serial/by-id/usb-Auterion_PX4_FMU_v6C.x_0-if00}"

if [[ ! -e "${device}" ]]; then
  echo "Pixhawk device not found: ${device}" >&2
  exit 1
fi

if command -v fuser >/dev/null 2>&1 && fuser "${device}" >/dev/null 2>&1; then
  echo "Pixhawk device is already open. Close QGroundControl and other serial consumers first:" >&2
  fuser -v "${device}" >&2 || true
  exit 1
fi

exec "${project_dir}/build/src/pixhawk_telemetry_hub" \
  --device "${device}" \
  --baud 115200 \
  --reconnect-delay 2 \
  --endpoint 127.0.0.1:14445 \
  --endpoint 127.0.0.1:14446 \
  --endpoint 127.0.0.1:14550
