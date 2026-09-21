#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

echo "=== [1/5] Configuring Q-Route ==="
cmake -S . -B build -DQRoute_BUILD_TESTS=ON -DQRoute_BUILD_BENCHMARKS=ON

echo "=== [2/5] Building Targets ==="
cmake --build build -j

echo "=== [3/5] Running Test Suite ==="
ctest --test-dir build --output-on-failure

echo "=== [4/5] Checking Architectural Invariants ==="
python3 scripts/check_invariants.py

echo "=== [5/5] Testing Q-Route CLI Execution ==="
if [ -f "./build/apps/qroute" ] || [ -f "./build/apps/qroute.exe" ]; then
    QROUTE_BIN="./build/apps/qroute"
    [ -f "./build/apps/qroute.exe" ] && QROUTE_BIN="./build/apps/qroute.exe"
    "${QROUTE_BIN}" --circuit data/circuit.qasm --coupling data/coupling.map --out build/routed.qasm
fi

echo "=== All Pipeline Steps Succeeded ==="