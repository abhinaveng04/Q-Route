# Course & Syllabus Mapping (BCSE0401: Data Structures and Algorithms - II)

This document provides a detailed mapping between the academic curriculum of **BCSE0401: Data Structures and Algorithms - II** and the architectural units of the **Q-Route** quantum compiler backend.

---

## 1. Master Curriculum Alignment Matrix

| Module | DSA-II Syllabus Topics | Q-Route Module | Primary Data Structures & Algorithms | Source Location | Tests & Benchmarks |
|---|---|---|---|---|---|
| **Module I** | Binary Trees, BST, AVL Trees, Threaded Binary Trees, Traversal Optimization | Unit 1 (Ingestion) | `ThreadedAST`, `ASTNode` (24B hot/cold split), `AvlSymbolTable` (LL, RR, LR, RL rotations) | `include/qroute/unit1_ast/`, `src/unit1_ast/` | `tests/test_unit1_ast.cpp`, `benchmarks/bench_traversal.cpp` (B1) |
| **Module II** | Graphs, Adjacency Lists, Traversals (BFS/DFS), Shortest Paths (Dijkstra, Bellman-Ford, Floyd-Warshall), MST (Prim, Kruskal) | Unit 2 (Graph & Routing) | `PhysicalCouplingGraph`, Binary Heap Dijkstra, `SwapSynthesizer` (3xCX decomposition), `PlacementMap` | `include/qroute/unit2_graph/`, `src/unit2_graph/` | `tests/test_unit2_graph.cpp`, `benchmarks/bench_dijkstra.cpp` (B2) |
| **Module III** | Dynamic Programming, Matrix Chain Multiplication (MCM), Optimal Parenthesization | Unit 3 (Contraction Planning) | `MatrixChainOptimizer`, $O(k^3)$ MCM DP recurrence, `SafeArithmetic` saturation | `include/qroute/unit3_dp/`, `src/unit3_dp/` | `tests/test_unit3_dp.cpp`, `benchmarks/bench_mcm.cpp` (B4) |
| **Module IV** | Backtracking, Branch & Bound, State Space Search, Admissible Bounding Functions | Unit 4 (Qubit Placement) | `QubitPlacementBAndB`, DFS B&B with explicit vector stack, `LookaheadGraph`, admissible lower bound | `include/qroute/unit4_bnb/`, `src/unit4_bnb/` | `tests/test_unit4_bnb.cpp`, `benchmarks/bench_bnb.cpp` (B5) |
| **Module V** | Advanced Data Structures, Comparative Priority Queues, Binomial & Fibonacci Heaps | Unit 5 (Telemetry Broker) | Custom `FibonacciHeap` ($O(1)$ amortized insert/decrease-key, cascading cuts), `BinaryHeapBaseline`, `TelemetryBroker` | `include/qroute/unit5_heap/`, `src/unit5_heap/` | `tests/test_unit5_heap.cpp`, `benchmarks/bench_priority_queues.cpp` (B3) |

---

## 2. Module I: Tree-Based Data Management & Traversals

### 2.1 Comparative Analysis: Tree Data Structures
| Feature / Characteristic | Standard BST | AVL Tree | Threaded Binary Tree | Binary Heap |
|---|---|---|---|---|
| **Worst-Case Search Time** | $O(N)$ (degenerate chain) | $O(\log N)$ (strict $|bf| \le 1$) | $O(N)$ (if unindexed) | $O(N)$ (no order on siblings) |
| **Worst-Case Insertion Time** | $O(N)$ | $O(\log N)$ | $O(N)$ | $O(\log N)$ |
| **In-Order Traversal Stack** | $O(h) = O(N)$ worst | $O(\log N)$ | **$O(1)$ auxiliary stack space** | $O(N \log N)$ (via heap sort) |
| **Memory Overhead** | 2 pointers (left, right) | 2 pointers + height/balance field | 2 pointers + 2 thread bits | 0 pointers (flat array) |
| **Q-Route Application** | Not used (unbalanced) | `AvlSymbolTable` for $O(\log V)$ identifier lookup | `ThreadedAST` for $O(1)$-stack instruction scan | Unit 2 Dijkstra priority queue |

### 2.2 Rotations & Rebalancing in `AvlSymbolTable`
Rebalancing occurs upon unwinding insert paths via iterative stack traversal:
1. **Left-Left (LL)**: Single Right Rotation at node $z$.
2. **Right-Right (RR)**: Single Left Rotation at node $z$.
3. **Left-Right (LR)**: Left Rotation on left child $y$, followed by Right Rotation on $z$.
4. **Right-Left (RL)**: Right Rotation on right child $y$, followed by Left Rotation on $z$.

---

## 3. Module II: Graph Modeling & Network Optimization

### 3.1 Adjacency Representation Trade-Offs
- **Adjacency Matrix**: $\Theta(|V|^2)$ space; edge lookup $\Theta(1)$; neighbor iteration $\Theta(|V|)$. Impractical for sparse NISQ hardware coupling graphs (degree $\le 4$).
- **Adjacency List (Chosen in `PhysicalCouplingGraph`)**: $\Theta(|V| + |E|)$ space; neighbor iteration $\Theta(\text{deg}(u))$; cache-friendly vector layout sorted by target vertex for deterministic edge enumeration.

### 3.2 Shortest Path & MST Algorithmic Comparison
| Algorithm | Problem Solved | Time Complexity | Q-Route Implementation / Role |
|---|---|---|---|
| **Dijkstra (Indexed Binary Heap)** | Single-Source Shortest Path (non-negative $w_e$) | $O(|E| \log |V|)$ | **Primary Routing Engine** (Unit 2): routes non-adjacent CX pairs minimizing cumulative $-\ln(F_e)$ |
| **Floyd-Warshall** | All-Pairs Shortest Path (APSP) | $\Theta(|V|^3)$ | **APSP Oracle** (Unit 2): precomputes all-pairs hop and weighted distance matrices for lower-bound bounds |
| **Bellman-Ford** | SSSP with arbitrary weights | $O(|V| \cdot |E|)$ | Evaluated; unnecessary since fidelity log-weights $w_e = -\ln(F_e) \ge 0$ |
| **Prim's Algorithm** | Minimum Spanning Tree (MST) | $O(|E| \log |V|)$ | Used to determine minimum communication backbone on physical subgraphs |
| **Kruskal's Algorithm** | Minimum Spanning Tree (Disjoint Set) | $O(|E| \log |E|)$ | Verified baseline for global topology connectivity analysis |

---

## 4. Module III: Dynamic Programming & Contraction Planning

### 4.1 Algorithmic Paradigms for Matrix Contraction
| Paradigm | Approach | Time Complexity | Space Complexity | Optimality Guarantee |
|---|---|---|---|---|
| **Greedy** | Contract smallest adjacent product first | $O(k \log k)$ | $O(k)$ | Sub-optimal; easily trapped in local minima |
| **Divide & Conquer** | Bisect chain into equal halves | $O(k \log k)$ | $O(k)$ | Sub-optimal; ignores dimension asymmetry |
| **Exhaustive Brute-Force** | Evaluate all Catalan parenthesizations | $\Theta\left(\frac{4^k}{k^{3/2}}\right)$ | $O(k)$ | Globally optimal; intractable for $k \ge 18$ |
| **Dynamic Programming (Chosen)** | Tabulate $m[i,j]$ in order of chain length $L$ | $\Theta(k^3)$ | $\Theta(k^2)$ | **Globally optimal; contracts $k=64$ in $<5$ ms** |

### 4.2 MCM Recurrence & 64-Bit Arithmetic Safety (INV-4)
$$m[i,j] = \begin{cases} 0, & i = j \\ \min\limits_{i \le \ell < j} \left\{ m[i,\ell] + m[\ell+1,j] + p_{i-1} p_\ell p_j \right\}, & i < j \end{cases}$$
Intermediate multiplications use `SafeArithmetic::saturated_triple_product_add`. Overflows saturate to $2^{64}-1$ with `overflow_detected = true`, guaranteeing portability across MSVC, GCC, and Clang without undefined behavior or compiler-dependent 128-bit extensions.

---

## 5. Module IV: Backtracking & Branch & Bound

### 5.1 Search Strategy Comparison
| Search Paradigm | State Exploration Order | Working Memory | Pruning Capability | Q-Route Compliance |
|---|---|---|---|---|
| **Standard Backtracking** | Depth-First Search (no bounds) | $O(n)$ | Feasibility checks only; cannot prune based on cost | Too slow ($O(m!)$) |
| **Best-First B&B (A*)** | Priority queue open list ($f = g + h$) | $O(b^d)$ (exponential memory) | Strong pruning | **Violates INV-2 (heap allocation in search)** |
| **DFS Branch & Bound (Chosen)** | Depth-First with global upper bound $B$ and admissible $LB$ | **$O(n + m)$ strictly bounded** | Strong pruning via $LB \ge B - \varepsilon$ | **Strictly satisfies INV-2: stack $\le n+1$** |

### 5.2 Admissible Bounding Function Proof
For any unassigned lookahead edge $(i,j)$ with weight $\alpha_{ij}$:
$$\delta_{ij} = \begin{cases} d(\pi(i), \pi(j)), & i,j \in \text{Assigned} \\ \min_{p \in \text{Unused}} d(\pi(i), p), & i \in \text{Assigned}, j \notin \text{Assigned} \\ \min_{p \in \text{Unused}} d(\pi(j), p), & j \in \text{Assigned}, i \notin \text{Assigned} \\ \text{min\_pair\_dist}, & i,j \notin \text{Assigned} \end{cases}$$
Because any valid completion must map unassigned logical qubits to unused physical qubits, the true distance in any completion is at least $\delta_{ij}$. Thus:
$$LB(\text{node}) = \text{cost}_{\text{assigned}} + \sum_{(i,j) \in E_L, i \notin A \lor j \notin A} \alpha_{ij} \delta_{ij} \le \text{True Optimal Cost}(\text{completion})$$
Hence $LB$ is strictly admissible, guaranteeing that pruning branches where $LB \ge B$ never discards an optimal assignment.

---

## 6. Module V: Advanced Priority Queues & Telemetry

### 6.1 Priority Queue & Advanced Search Tree Comparison
| Data Structure | `insert` (Amortized) | `decrease_key` (Amortized) | `extract_min` (Amortized) | Memory per Node | Cache Locality |
|---|---|---|---|---|---|
| **Binary Heap** | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | Flat array (1 element) | Excellent (contiguous) |
| **Binomial Heap** | $O(1)$ | $O(\log N)$ | $O(\log N)$ | 3 pointers + degree | Moderate |
| **Fibonacci Heap (Chosen for Unit 5)** | **$O(1)$** | **$O(1)$** | **$O(\log N)$** | 4 pointers + degree + mark | Pointer-based (node pool recycled) |
| **Red-Black Tree** | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | 2 pointers + parent + color | Poor |
| **B / B+ Tree** | $O(\log N)$ | $O(\log N)$ | $O(\log N)$ | Block-allocated keys/ptrs | High block locality (disk/DB) |

### 6.2 Pedagogical Justification for Heap Selection
- **Unit 2 Dijkstra Routing (INV-1)**: Dijkstra's algorithm on sparse physical coupling graphs ($|V| \le 127$) performs frequent `extract_min` relative to edge relaxations. `BinaryHeapBaseline` outperforms Fibonacci Heap due to flat contiguous memory layout and zero pointer indirection.
- **Unit 5 Telemetry Broker (FH-1..FH-13)**: The telemetry broker tracks physical qubit fidelity degradation ($s_q = \text{critical\_time} - \text{elapsed\_time}$). Every gate executed across the QPU invokes `update_slack`, performing frequent `decrease_key` operations. Here, Fibonacci Heap's $O(1)$ amortized `decrease_key` avoids the $O(\log N)$ sift-up bottleneck of binary heaps, achieving 1,261,198 ops/sec in benchmark B3.
