# Q-Route Architectural Blueprint (Revision R2)

## 1. System Overview

Q-Route is a quantum compiler backend designed to solve the Quantum Neural Network (QNN) routing problem under physical quantum hardware coupling constraints with non-unitary fidelity degradation.

## 2. End-to-End Pipeline Architecture

```
                        +-------------------------------------------+
                        |            data/circuit.qasm              |
                        |        OpenQASM 2.0 QNN input trace       |
                        +---------------------+---------------------+
                                              |
                                              v
                        +-------------------------------------------+
                        |        Unit 1: Lexer / Parser             |
                        |  AVL Symbol Table (O(log n) identifiers)  |
                        +---------------------+---------------------+
                                              |
                                              v
                        +-------------------------------------------+
                        |   Unit 1: ThreadedAST (hot/cold split)    |
                        |   O(1)-auxiliary inorder traversal        |
                        +---------------------+---------------------+
                                              |
                                              v
                        +-------------------------------------------+
                        |   Lookahead Interaction Graph Builder     |
                        |   logical edges + weights alpha_ij        |
                        +---------------------+---------------------+
                                              |
                                              v
        +-------------------------------------+--------------------------------------+
        |  Unit 4: DFS Branch & Bound Placement  [INV-2]                             |
        |  explicit std::vector stack, global upper bound B                          |
        |  min  sum_{(i,j) in G_lookahead} alpha_ij * dist(pi(i), pi(j))             |
        +-------------------------------------+--------------------------------------+
                                              |  initial mapping pi : L -> P
                                              v
        +-------------------------------------+--------------------------------------+
        |  Unit 2: Routing Engine  [INV-1]                                           |
        |  PhysicalCouplingGraph (adjacency list, w_ij = -ln F_ij)                   |
        |  Dijkstra via std::priority_queue ONLY                                     |
        |  SwapSynthesizer -> 3x CX per SWAP + PlacementMap update                   |
        +-------------------------------------+--------------------------------------+
                                              |  physical instruction stream
                                              v
        +-------------------------------------+--------------------------------------+
        |  Unit 3: MatrixChainOptimizer  [INV-3]                                     |
        |  PLANNING ONLY: m[i,j], s[i,j], parenthesization, savings                  |
        |  uint64_t + SafeArithmetic saturation  [INV-4]                             |
        +-------------------------------------+--------------------------------------+
                                              |
                                              v
        +-------------------------------------+--------------------------------------+
        |  Deterministic Telemetry Model                                             |
        |  gate durations -> per-qubit elapsed time -> T1/T2 fidelity slack          |
        +-------------------------------------+--------------------------------------+
                                              |  keys = remaining slack
                                              v
        +-------------------------------------+--------------------------------------+
        |  Unit 5: TelemetryBroker + custom FibonacciHeap                            |
        |  O(1) amortized insert / decrease_key, O(log n) amortized extract_min      |
        +-------------------------------------+--------------------------------------+
                                              |
                                              v
        +-------------------------------------+--------------------------------------+
        |  Outputs: routed physical circuit, deterministic report, MCM plan,         |
        |           decoherence alert log, CPU timing telemetry, benchmark tables    |
        +----------------------------------------------------------------------------+
```

## 3. Non-Negotiable Invariant Rules (R2 Binding)

1. **INV-1 (Unit 2 Routing)**: Unit 2 MUST use `std::priority_queue` (binary heap) exclusively. The custom `FibonacciHeap` is strictly forbidden in any Unit 2 translation unit. Tie-breaking is deterministic via `std::pair<double, PhysicalQubit>` with `std::greater<>`.
2. **INV-2 (Unit 4 Placement)**: Unit 4 MUST use Depth-First Search Branch & Bound with an explicit `std::vector` stack and a global upper-bound register `B`. No best-first open list, no `std::priority_queue`, no `FibonacciHeap`. Max stack depth $\le n$, working memory $O(n + m)$.
3. **INV-3 (Unit 3 Planning)**: Unit 3 is strictly for contraction planning. Numerical quantum simulation, `std::complex`, state vectors, amplitudes, and BLAS/Eigen are strictly forbidden.
4. **INV-4 (Common Arithmetic)**: 128-bit compiler integer extensions (`__int128`, `_umul128`, `uint128_t`) are strictly forbidden across the codebase. All overflow protection uses `uint64_t` arithmetic with explicit bounds detection and saturation in `SafeArithmetic`.
5. **MEM (Dynamic Memory)**: Raw dynamic memory allocation (`new`, `delete`, `malloc`, `free`) is forbidden in production code (`include/`, `src/`, `apps/`). RAII containers (`std::vector`, `std::unique_ptr`, `std::deque`) manage all heap allocations.

## 4. Design Decisions

- **Threaded AST In-Order Traversal**: Uses null child pointer threading to achieve $O(1)$ auxiliary space during traversal, avoiding recursion and call stack exhaustion on deep circuits.
- **Hot/Cold Memory Split**: `ASTNode` (24 bytes) is packed for cache locality during traversal; `GatePayload` (64 bytes) is stored contiguously in a secondary array and accessed only when executing instructions.
- **Deterministic Reporting**: All floating point outputs and metrics use fixed precision and deterministic tie-breaking to ensure reproducible outputs across runs.