#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

# Detect python binary
PYTHON_BIN="python3"
if ! command -v python3 &>/dev/null; then
    if command -v python &>/dev/null; then
        PYTHON_BIN="python"
    else
        echo "Error: python not found" >&2
        exit 1
    fi
fi

echo "=== [1/7] Configuring Q-Route ==="
cmake -S . -B build -DQRoute_BUILD_TESTS=ON -DQRoute_BUILD_BENCHMARKS=ON

echo "=== [2/7] Building Targets ==="
cmake --build build -j

echo "=== [3/7] Running Test Suite ==="
ctest --test-dir build --output-on-failure

echo "=== [4/7] Checking Architectural Invariants ==="
"${PYTHON_BIN}" scripts/check_invariants.py

echo "=== [5/7] Testing Q-Route CLI Execution ==="
QROUTE_BIN="./build/apps/qroute"
if [ -f "./build/apps/qroute.exe" ]; then
    QROUTE_BIN="./build/apps/qroute.exe"
fi

"${QROUTE_BIN}" --circuit data/circuit.qasm --coupling data/coupling.map --out build/routed.qasm --out-json build/report.json > build/report_run.txt

echo "=== [6/7] Running Benchmark Suite ==="
"${QROUTE_BIN}" --bench

echo "=== [7/7] Verifying Golden Regression Diff ==="
"${PYTHON_BIN}" scripts/golden_diff.py tests/golden/report.golden.txt build/report_run.txt

echo "=== All Pipeline Steps Succeeded ==="

