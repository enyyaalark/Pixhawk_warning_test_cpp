#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
run="${root}/.run"
mkdir -p "${run}" "${root}/logs"
if [[ -f "${run}/hub.pid" ]] && kill -0 "$(<"${run}/hub.pid")" 2>/dev/null; then echo "System already running" >&2; exit 1; fi
nohup "${root}/scripts/start_telemetry_hub.sh" >"${root}/logs/telemetry_hub.log" 2>&1 & echo $! >"${run}/hub.pid"
sleep 1
nohup "${root}/build/src/pixhawk_monitor" --udp-port 14446 --udp-bind 127.0.0.1 >"${root}/logs/warning_service.log" 2>&1 & echo $! >"${run}/warning.pid"
echo "Hub and warning service started. Start PFD separately with --udp-port 14445."
