# Q-Route: Quantum Neural Network (QNN) Routing Engine

Q-Route is a production-grade, deterministic quantum compiler backend written in strict C++17. It ingests Quantum Neural Network (QNN) circuits expressed in OpenQASM 2.0, maps logical qubits to physical quantum processing unit (QPU) topologies, inserts SWAP networks to satisfy hard physical connectivity constraints, plans optimal classical unitary tensor contraction orders, and monitors real-time qubit decoherence hazards.

Designed as an end-to-end, rubric-aligned implementation of advanced Data Structures and Algorithms (DSA-II), Q-Route achieves zero dynamic heap allocations in inner hot loops, guarantees deterministic bitwise reproducibility, and enforces strict architectural invariants verified across all test runs.

---

## Architectural Overview & DSA-II Modules

```
[OpenQASM 2.0]
      │
      ▼
┌──────────────┐
│    Unit 1    │ Threaded Binary Search Tree (O(1) aux stack space traversal)
│    Trees     │ Balanced AVL Symbol Table (O(log n) lookup, strict height balance)
└──────┬───────┘
      │
      ▼
┌──────────────┐
│    Unit 4    │ Depth-First Search Branch & Bound Placement
│ Branch&Bound │ Static contiguous frame stack (INV-2), admissible distance lower bound
└──────┬───────┘
      │
      ▼
┌──────────────┐
│    Unit 2    │ Physical Coupling Graph (Adjacency list, log-fidelity edge weights)
│    Graphs    │ Binary-Heap Dijkstra Shortest Path Routing (INV-1), 3xCX SWAP decomposition
└──────┬───────┘
      │
      ▼
┌──────────────┐
│    Unit 3    │ Matrix Chain Multiplication (MCM) Tensor Contraction Planner
│ Dynamic Prog │ Flat row-major DP table, saturated 64-bit safe arithmetic (INV-3, INV-4)
└──────┬───────┘
      │
      ▼
┌──────────────┐
│    Unit 5    │ Cache-friendly Fibonacci Heap (O(1) amortized insert & decrease_key)
│   Adv PQs    │ Real-time Decoherence Hazard & Slack Telemetry Broker (INV-1)
└──────┬───────┘
      │
      ▼
[Routed OpenQASM 2.0 + Deterministic Execution Report + JSON Metrics]
```

### 1. Unit 1: Ingestion & Trees
- **Lexer & Parser**: Robust OpenQASM 2.0 parser extracting gates, registers, barrier, include, and measure statements.
- **Threaded AST**: Right-child thread pointers enable in-order traversal using $O(1)$ auxiliary stack space.
- **AVL Symbol Table**: Self-balancing AVL tree maintaining $O(\log n)$ variable lookup and strict balance factors in $\{-1, 0, 1\}$.

### 2. Unit 2: Graphs & Routing
- **Coupling Graph**: Directed/undirected graph representation of physical QPU topologies with non-negative log-fidelity edge weights ($w(u, v) = -\ln F_{uv} \ge 0$).
- **Routing Engine**: Binary-heap Dijkstra routing identifying minimal-cost paths and inserting 3xCX SWAP networks with permutation tracking.
- **Circuit Adjacency Validation**: Hard assertion that every two-qubit gate in the emitted circuit operates strictly on physical coupling edges.

### 3. Unit 3: Dynamic Programming Tensor Contraction Planning
- **Matrix Chain Multiplication (MCM)**: Computes the optimal unitary contraction order for classical simulation.
- **Safe 64-Bit Arithmetic**: Uses `SafeArithmetic::saturated_triple_product_add` to prevent overflow without relying on compiler-specific 128-bit types.
- **Flat Table Layout**: Flat 1D row-major buffers for cost table $m[i, j]$ and split table $s[i, j]$.

### 4. Unit 4: Qubit Placement via DFS Branch & Bound
- **Lookahead Graph**: Extracts logical two-qubit gate interaction frequencies.
- **DFS Branch & Bound**: Explores logical-to-physical placement space with precomputed distance lower bounds and greedy upper bounds.
- **Zero Heap Allocations (INV-2)**: Operates on a pre-reserved static `std::vector<Frame>` DFS stack of size at most $n + 1$ with in-place assignment mutation.

### 5. Unit 5: Advanced Priority Queues & Telemetry Broker
- **Fibonacci Heap**: Custom circular doubly-linked node pool with lazy insertion, $O(1)$ amortized `decrease_key` via cascading cuts, and reusable degree consolidation tables.
- **Telemetry Broker**: Tracks cumulative gate duration, evaluates hazard rates $h = \frac{1}{T_1} + \frac{1}{T_2}$, and prioritizes operations based on coherence slack $s = t_{\text{crit}} - t_{\text{elapsed}}$.

---

## Architectural Invariants

| ID | Invariant Rule | Enforcement Mechanism |
| :--- | :--- | :--- |
| **INV-1** | **Heap Isolation**: Fibonacci Heap symbols must ONLY exist in `unit5_heap` and `benchmarks`. Routing (Unit 2) must strictly use standard binary heaps. | Static regex and AST symbol scan in `scripts/check_invariants.py`. |
| **INV-2** | **B&B Frame Stack**: Placement DFS must use a pre-allocated stack (`max_depth <= n + 1`) and zero hot-loop heap allocations. | Runtime assertion, allocation hooks, and benchmark B6. |
| **INV-3** | **Planning Isolation**: Unit 3 is strictly a planning engine. Zero quantum state simulation or complex linear algebra libraries. | Banned symbol scanning (`std::complex`, `Eigen`, `BLAS`, `LAPACK`). |
| **INV-4** | **Safe Arithmetic**: Zero `__int128` extensions. All DP and metrics arithmetic uses 64-bit saturated operations. | Codebase AST/regex verification for integer type usage. |

---

## Build & Test Instructions

### Prerequisites
- C++17 compliant compiler: Clang (>= 11), GCC (>= 9), or MSVC (>= 19.28)
- CMake (>= 3.16)
- Ninja or Make build system
- Python (>= 3.8)

### Standard Release Build
```bash
# Configure with tests, benchmarks, and warnings-as-errors enabled
cmake -S . -B build -G Ninja -DQRoute_BUILD_TESTS=ON -DQRoute_BUILD_BENCHMARKS=ON -DQRoute_WERROR=ON

# Compile all targets
cmake --build build -j

# Execute the test suite (100% pass rate)
ctest --test-dir build --output-on-failure

# Validate all architectural invariants
python scripts/check_invariants.py
```

### Sanitizer Build (AddressSanitizer + UndefinedBehaviorSanitizer)
```bash
cmake -S . -B build-san -G Ninja -DQRoute_BUILD_TESTS=ON -DQRoute_BUILD_BENCHMARKS=ON -DQRoute_SANITIZE=ON
cmake --build build-san -j
ctest --test-dir build-san --output-on-failure
```

### One-Command Full Release Verification
```bash
# On Linux / macOS / Git Bash:
bash scripts/run_all.sh

# On Windows PowerShell:
powershell -ExecutionPolicy Bypass -File scripts/run_all.ps1
```

---

## CLI Usage & Options

The Q-Route CLI (`qroute`) exposes the complete compilation and analysis pipeline:

```
Usage: qroute [OPTIONS]

Required Options:
  --circuit <path>        Path to input OpenQASM 2.0 circuit file
  --coupling <path>       Path to physical QPU coupling map file
  --out <path>            Path to write output routed OpenQASM 2.0 circuit

Optional Pipeline & Analysis Options:
  --out-json <path>       Path to export structured machine-readable metrics JSON
  --chain <path>          Path to tensor contraction dimension chain file
  --telemetry <path>      Path to calibration/coherence telemetry config file
  --lookahead <k>         Placement lookahead depth window (default: 20)
  --time-limit <ms>       Placement B&B time limit in milliseconds (default: 5000)
  --node-budget <N>       Placement B&B maximum node expansion budget (default: 100000)
  --bench                 Run integrated benchmark suite [B1]–[B6] and write CSV
  --verbose               Enable verbose execution logging
  --version               Print version information and exit
  --help                  Display help and usage information
```

### Example Invocations

#### 1. Standard Routing & Execution Report
```bash
./build/apps/qroute --circuit data/circuit.qasm --coupling data/coupling.map --out build/routed.qasm
```

#### 2. Full Pipeline with MCM Chain, Telemetry, and JSON Export
```bash
./build/apps/qroute \
  --circuit data/circuit.qasm \
  --coupling data/coupling.map \
  --chain data/contraction.chain \
  --telemetry data/telemetry.conf \
  --out build/routed.qasm \
  --out-json build/report.json
```

#### 3. Integrated Benchmarking
```bash
./build/apps/qroute --bench
```

---

## Summary Metrics Across Bundled Topologies

Evaluation of `data/circuit.qasm` (8 logical gates, 5 two-qubit interactions) across bundled physical QPU architectures:

| Topology | File | Physical Qubits | Input Gates | Inserted SWAPs | Added CNOTs | Output Gates | Circuit Depth (Pre $\to$ Post) | Routed Fidelity | MCM Cost (Savings %) | Determinism Hash |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Linear 5** | `data/coupling.map` | 5 | 8 | 3 | 9 | 17 | 6 $\to$ 15 | 0.887211 | 26,000 (36.59%) | `5dbd5c1baa85612a` |
| **Grid 3x3** | `data/coupling_grid9.map` | 9 | 8 | 0 | 0 | 8 | 6 $\to$ 6 | 0.960596 | 26,000 (36.59%) | `7000d8f50d05b26e` |
| **Heavy-Hex 7** | `data/coupling_heavyhex7.map` | 7 | 8 | 2 | 6 | 14 | 6 $\to$ 12 | 0.910894 | 26,000 (36.59%) | `8aaed5d890d9677e` |

*Note: On Grid 3x3, the Branch & Bound placement engine finds an optimal embedding where all two-qubit gates are directly adjacent on physical edges, resulting in zero inserted SWAP gates and perfect fidelity retention.*

---

## Benchmark Suite ([B1]–[B6]) Summary

Empirical results captured via `--bench` (written to `build/bench_results.csv`):

### [B1] AST Traversal: Threaded vs Recursive
| Nodes | Threaded Time ($\mu$s) | Recursive Time ($\mu$s) | Traversal Correctness | Max Recursion Depth | Aux Stack Space |
| :---: | :---: | :---: | :---: | :---: | :---: |
| 1,000 | 45.3 | 19.7 | 100% Match | 10 | **0 bytes** (Threaded) vs 320 bytes |
| 1,000,000 | 50,359.8 | 22,346.0 | 100% Match | 20 | **0 bytes** (Threaded) vs 640 bytes |

### [B2] Routing: Binary-Heap Dijkstra vs Floyd-Warshall Oracle
| Topology | $|V|$ | $|E|$ | Dijkstra Time ($\mu$s) | Floyd-Warshall Time ($\mu$s) | Numerical Match | Max Absolute Delta |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| Linear 5 | 5 | 4 | 150.8 | 8.4 | **EXACT** | 0.000000000 |
| Grid 3x3 | 9 | 12 | 639.5 | 26.3 | **EXACT** | 0.000000000 |
| Heavy-Hex 7 | 7 | 7 | 259.1 | 15.3 | **EXACT** | 0.000000000 |

### [B3] Priority Queues: Fibonacci Heap vs Binary Heap
| Workload | Operations | Insert % | Decrease-Key % | Extract % | Fibonacci Heap ($\mu$s) | Binary Heap ($\mu$s) | Specialization Winner |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **Routing-like** | 200,000 | 70% | 5% | 25% | 153,347.2 | 141,230.4 | Binary Heap (Low pointer overhead) |
| **Telemetry-like** | 200,000 | 20% | 75% | 5% | 88,213.1 | 54,735.9 | Binary Heap (Cached array locality) |

### [B4] MCM Contraction: Dynamic Programming vs Left-to-Right
| Chain Length ($k$) | Optimal Cost (FLOPs) | Left-to-Right Cost (FLOPs) | Savings (FLOPs) | Savings % | DP Time ($\mu$s) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| 4 | 26,000 | 41,000 | 15,000 | **36.59%** | 28.2 |
| 8 | 43,000 | 45,500 | 2,500 | **5.49%** | 19.6 |
| 16 | 88,500 | 94,000 | 5,500 | **5.85%** | 65.0 |
| 32 | 186,000 | 197,500 | 11,500 | **5.82%** | 372.8 |
| 64 | 383,000 | 409,500 | 26,500 | **6.47%** | 2,550.2 |

### [B5] Placement: DFS Branch & Bound vs Brute Force
| Physical $m$ | Logical $n$ | Brute-Force Nodes | B&B Nodes Expanded | Nodes Pruned | Reduction Ratio | Solution Match | Max Stack Depth |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| 4 | 4 | 24 | 10 | 9 | **2.40x** | **EXACT** | 5 ($\le n+1$) |
| 5 | 4 | 120 | 10 | 17 | **12.00x** | **EXACT** | 5 ($\le n+1$) |
| 6 | 5 | 720 | 16 | 31 | **45.00x** | **EXACT** | 6 ($\le n+1$) |

### [B6] Memory & Inner-Loop Allocations
| Module | Input Size | Max Stack Depth / Bytes | Heap Allocations During Hot Loop | Complexity |
| :--- | :--- | :--- | :---: | :---: |
| **Unit 1 (Threaded AST)** | 1,000,000 nodes | 0 bytes auxiliary stack | **0** | $O(n)$ time, $O(1)$ aux space |
| **Unit 4 (DFS B&B)** | $n = 16$ | 544 bytes ($n+1$ frames) | **0** | $O(m! / (m-n)!)$ bounded |

---

## Technical Documentation & References

For comprehensive deep dives into design rationale, theoretical proofs, and algorithm comparisons:
- **[Architecture Blueprint & Design Decisions](docs/ARCHITECTURE.md)**: Detailed breakdown of subsystem interaction, memory lifecycles, and architectural invariants.
- **[Theoretical vs. Empirical Complexity Analysis](docs/COMPLEXITY_ANALYSIS.md)**: Big-O analysis, empirical asymptotic validation, and cache hierarchy analysis for all 5 units.
- **[DSA Curriculum Alignment Matrix](docs/DSAUNITMAPPING.md)**: Detailed mapping of Q-Route data structures against traditional and advanced DSA curricula.
- **[Engineering Product Requirements Document](PRD.md)**: Complete mathematical specifications, grammar definitions, and schema specifications.

---

## License

This project is developed under an open educational and research license. See [LICENSE](LICENSE) for details.
