# Q-Route Architectural Blueprint (Revision R2)

## 1. System Overview

Q-Route is a production-grade quantum compiler backend designed to solve the Quantum Neural Network (QNN) mapping, placement, and routing problems under physical quantum coupling constraints and non-unitary gate fidelity degradation.

The system ingests OpenQASM 2.0 instruction traces and physical coupling topologies, deterministically computes optimal initial qubit placements via DFS Branch & Bound, routes multi-qubit interactions via Dijkstra shortest-path SWAP synthesis, derives optimal tensor contraction schedules via Dynamic Programming Matrix Chain Multiplication, and monitors qubit decoherence health via a Fibonacci Heap priority queue broker.

---

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

---

## 3. Non-Negotiable Invariant Rules (R2 Binding)

1. **INV-1 (Unit 2 Routing)**: Unit 2 MUST use `std::priority_queue` (binary heap) exclusively. The custom `FibonacciHeap` is strictly forbidden in any Unit 2 translation unit. Tie-breaking is deterministic via `std::pair<double, PhysicalQubit>` with `std::greater<>`.
2. **INV-2 (Unit 4 Placement)**: Unit 4 MUST use Depth-First Search Branch & Bound with an explicit `std::vector` stack and a global upper-bound register `B`. No best-first open list, no `std::priority_queue`, no `FibonacciHeap`. Max stack depth $\le n + 1$, working memory $O(n + m)$. Zero heap allocations in the DFS hot loop.
3. **INV-3 (Unit 3 Planning)**: Unit 3 is strictly for contraction planning. Numerical quantum simulation, `std::complex`, state vectors, amplitudes, and BLAS/Eigen are strictly forbidden.
4. **INV-4 (Common Arithmetic)**: 128-bit compiler integer extensions (`__int128`, `_umul128`, `uint128_t`) are strictly forbidden across the codebase. All overflow protection uses `uint64_t` arithmetic with explicit bounds detection and saturation in `SafeArithmetic`.
5. **MEM (Dynamic Memory)**: Raw dynamic memory allocation (`new`, `delete`, `malloc`, `free`) is forbidden in production code (`include/`, `src/`, `apps/`). RAII containers (`std::vector`, `std::unique_ptr`, `std::deque`) manage all heap allocations.

---

## 4. Architectural Design Decisions

### 4.1 Threaded AST In-Order Traversal & Cache Packing (Unit 1)
- **$O(1)$ Auxiliary Space Traversal**: Standard recursive binary tree traversal requires $O(h)$ auxiliary stack frames ($O(N)$ worst-case for degenerate trees). To ensure immunity against stack overflow on circuits containing millions of gates, `ThreadedAST` replaces null child pointers with in-order predecessor/successor threads. Traversal operates in amortized $O(1)$ time per node using zero recursion.
- **Hot/Cold Memory Split**:
  - `ASTNode` (24 bytes): Contains only child pointers, thread flags, source line, and opcode. Packs ~2.67 nodes per 64-byte L1 cache line, maximizing cache line utilization during structural traversal.
  - `GatePayload` (64 bytes): Contains arguments, gate parameters, and classical registers. Stored in a separate contiguous pool and dereferenced only when instruction execution occurs.

### 4.2 Lookahead Interaction Graph & Branching Order (Unit 4)
- **Symmetric Interaction Graph**: Tracks interaction weights $\alpha_{ij}$ representing the number of two-qubit gates between logical qubits $i$ and $j$ within the lookahead window (or full trace if window = 0).
- **Deterministic Degree-Sorted Variable Branching**: Logical qubits are branched in descending order of their total lookahead interaction degree, breaking ties by ascending logical qubit ID. Edges are sorted lexicographically by $(i, j)$ for strictly reproducible search space exploration.

### 4.3 DFS Branch & Bound Invariants & Admissibility (Unit 4)
- **Zero Heap Allocations in Hot Loop**: Search state is maintained in-place using pre-allocated arrays `current_l2p_` (size $n$) and `current_used_` (size $m$). Transitions use `assign()` and `undo_assign()` with $O(1)$ overhead.
- **Precomputed Lower Bound Accelerators**: Pairwise physical shortest paths and sorted distance vectors (`sorted_dist_from_`) are precomputed once before search. Computing `min_distance_to_unused(p)` performs an $O(1)$ to $O(m)$ scan across pre-sorted candidates rather than an $O(m^2)$ distance recalculation.
- **Stack Depth Bound**: The explicit DFS stack is reserved once to $n + 1$ frames. Max stack depth is asserted $\le n + 1$ upon search completion.

### 4.4 Graph Modeling, Routing & SWAP Synthesis (Unit 2)
- **Additive Weight Metric**: Physical edges are weighted as $w_e = -\ln(F_e)$. Because $\prod F_i = \exp(-\sum -\ln F_i)$, shortest-path routing via Dijkstra with additive weights mathematically maximizes cumulative multi-qubit fidelity.
- **Strict SWAP Canonicalization**: Every non-adjacent two-qubit gate is routed by decomposing each hop into exactly 3 physical CNOT gates ($\text{CX}(u,v) \to \text{CX}(v,u) \to \text{CX}(u,v)$).
- **Write-Time Hard Adjacency Assertion**: At write time, `RoutedCircuit::to_qasm_text` checks physical adjacency for every emitted CX gate, guaranteeing that non-adjacent physical gates can never reach output.

### 4.5 Dynamic Programming Tensor Contraction Planning (Unit 3)
- **Planning vs. Simulation Separation (INV-3)**: Unit 3 strictly plans contraction orders for quantum state networks without performing numerical matrix tensor contractions or using linear algebra libraries.
- **Contiguous 1D Flat Buffers**: Matrix cost table $m[i,j]$ and split table $s[i,j]$ are stored in flat contiguous row-major 1D vectors of size $n \times n$, providing optimal cache prefetching over nested pointer-to-pointer structures.
- **Saturation Arithmetic (INV-4)**: Intermediate triple products $p_{i-1} p_\ell p_j$ and sum accumulations are calculated via `SafeArithmetic::saturated_triple_product_add`. Overflows saturate to `std::numeric_limits<uint64_t>::max()`, set `overflow_detected = true`, and print `SATURATED` rather than triggering undefined integer overflow.

### 4.6 Fibonacci Heap Telemetry Broker & Data Structures (Unit 5)
- **Node Pool & Stable Pointers (FH-10, FH-11)**: `FibonacciHeap` maintains nodes inside a stable `std::deque<Node>` pool. Nodes removed by `extract_min` are recycled into a `free_list_` to minimize OS heap allocations.
- **Reusable Scratch Buffers (FH-4)**: `degree_table_` (sized to $\lfloor \log_\phi(N) \rfloor + 2$) and `roots_buffer_` are pre-allocated members of `FibonacciHeap`, reused across all consolidations to achieve zero dynamic memory allocations during `extract_min`.
- **Amortized Efficiency Separation**: In benchmarks, `routing_like` (extract-heavy) workload favors array-backed `BinaryHeapBaseline` due to cache locality, while `telemetry_like` (decrease-key heavy) workload favors `FibonacciHeap` due to $O(1)$ amortized decrease-key.

### 4.7 Reproducibility & Output Standardization
- **Locale Independence**: `std::locale::classic()` is explicitly installed at `main()` entry, guaranteeing standard C decimal formatting (`.` separator) regardless of host system configuration.
- **Binary-Mode File I/O**: Files are read and written in binary mode with normalized LF line endings (`\n`), ensuring byte-for-byte regression equality between Linux and Windows.
- **64-Bit FNV-1a Determinism Hash**: All non-timing stdout report fields are hashed sequentially via 64-bit FNV-1a, providing an instant tamper-evident checksum verifying end-to-end algorithmic determinism across repeated executions.
