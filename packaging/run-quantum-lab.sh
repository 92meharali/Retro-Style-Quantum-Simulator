#!/usr/bin/env bash
# Launcher for portable Quantum Circuit Lab builds.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
exec "$ROOT/quantum-lab" "$ROOT/presets"
