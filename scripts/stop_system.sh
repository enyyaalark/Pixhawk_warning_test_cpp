#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"; run="${root}/.run"
for name in warning hub; do f="${run}/${name}.pid"; if [[ -f "$f" ]]; then pid="$(<"$f")"; kill -TERM "$pid" 2>/dev/null || true; rm -f "$f"; fi; done
echo "System stop requested."
