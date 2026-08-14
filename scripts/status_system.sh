#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"; run="${root}/.run"
for name in hub warning; do f="${run}/${name}.pid"; if [[ -f "$f" ]] && kill -0 "$(<"$f")" 2>/dev/null; then echo "$name: RUNNING pid=$(<"$f")"; else echo "$name: STOPPED"; fi; done
ss -lunp 2>/dev/null | grep -E '14445|14446|14550' || true
