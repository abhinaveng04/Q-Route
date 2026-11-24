# Q-Route Full Automated Release Verification Pipeline (PowerShell)
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$rootDir = Split-Path -Parent $scriptDir
Set-Location $rootDir

Write-Host "=== [1/7] Configuring Q-Route ==="
cmake -S . -B build -DQRoute_BUILD_TESTS=ON -DQRoute_BUILD_BENCHMARKS=ON

Write-Host "=== [2/7] Building Targets ==="
cmake --build build -j

Write-Host "=== [3/7] Running Test Suite ==="
ctest --test-dir build --output-on-failure

Write-Host "=== [4/7] Checking Architectural Invariants ==="
python scripts/check_invariants.py

Write-Host "=== [5/7] Testing Q-Route CLI Execution ==="
$qrouteBin = "./build/apps/qroute"
if (Test-Path "./build/apps/qroute.exe") {
    $qrouteBin = "./build/apps/qroute.exe"
}

# Run binary in utf8 output
& $qrouteBin --circuit data/circuit.qasm --coupling data/coupling.map --out build/routed.qasm --out-json build/report.json | Out-File -FilePath build/report_run.txt -Encoding utf8

Write-Host "=== [6/7] Running Benchmark Suite ==="
& $qrouteBin --bench

Write-Host "=== [7/7] Verifying Golden Regression Diff ==="
python scripts/golden_diff.py tests/golden/report.golden.txt build/report_run.txt

Write-Host "=== All Pipeline Steps Succeeded ==="
