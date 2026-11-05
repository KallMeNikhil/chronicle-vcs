#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make clean
make test
if [ "${1:-}" = "--asan" ]; then
  make asan
fi
echo "check: OK"
