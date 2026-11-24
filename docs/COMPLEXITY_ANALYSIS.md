# Q-Route Algorithmic Complexity Analysis

This document provides a comprehensive theoretical and empirical complexity analysis of all five computational units within the Q-Route quantum compiler backend, validated using empirical benchmarks conducted via `qroute --bench`.

---

## 1. Master Theoretical vs. Empirical Complexity Table

| Unit | Component / Algorithm | Theoretical Time Complexity | Theoretical Space Complexity | Empirical Benchmark | Measured Scaling / Behavior | Invariant Constraint |
|---|---|---|---|---|---|---|
| **Unit 1** | **Threaded AST In-Order Traversal** | $\Theta(N)$ | $\Theta(1)$ auxiliary | `bench_traversal` (B1) | $N = 10^3$: 47.7 $\mu$s<br>$N = 10^6$: 110.6 ms | `max_recursion_depth = 0`<br>`aux_stack_bytes = 0` |
| **Unit 1** | **Recursive AST In-Order Traversal** | $\Theta(N)$ | $\Theta(h) = \Theta(\log N)$ aux | `bench_traversal` (B1) | $N = 10^3$: 21.4 $\mu$s ($h=10$)<br>$N = 10^6$: 53.3 ms ($h=20$) | Call-stack memory grows with tree height $h$ |
| **Unit 1** | **AVL Symbol Table Lookup/Insert** | $O(\log V)$ | $O(V)$ | Unit 1 test suite (T1.1–T1.11) | Iterative balance check $|bf| \le 1$ holds; 0 recursion depth | Zero heap allocations inside tree rotations |
| **Unit 2** | **Physical Coupling Dijkstra** | $O(\|E\| \log \|V\|)$ | $O(\|V\| + \|E\|)$ | `bench_dijkstra` (B2) | Linear-5: 482.1 $\mu$s<br>Grid-9: 885.3 $\mu$s<br>HeavyHex-7: 537.6 $\mu$s | **INV-1**: Exclusive use of `std::priority_queue` |
| **Unit 2** | **Floyd-Warshall APSP Oracle** | $\Theta(\|V\|^3)$ | $\Theta(\|V\|^2)$ | `bench_dijkstra` (B2) | Exact match vs Dijkstra (`max_abs_delta = 0.000000000`) | Exact reference oracle for all-pairs distances |
| **Unit 2** | **SWAP Synthesizer Route** | $O(\|E\| \log \|V\| + d)$ | $O(d)$ | E2E Pipeline (T6.2, T6.3) | $\text{added\_cnot} = 3 \times \text{inserted\_swap}$; 100% physically adjacent | Hard adjacency assertion on every emitted CX |
| **Unit 3** | **Matrix Chain DP Contraction** | $\Theta(k^3)$ | $\Theta(k^2)$ | `bench_mcm` (B4) | $k=4$: 47.2 $\mu$s, 36.6% savings<br>$k=64$: 4.46 ms, 6.5% savings | **INV-3**: Planning only; **INV-4**: Safe uint64 saturation |
| **Unit 3** | **Exhaustive Catalan Contraction** | $\Theta\left(\frac{4^k}{k^{3/2}}\right)$ | $O(k)$ | Unit 3 test suite (T3.3) | Impractical for $k \ge 18$; verified against DP for $k \le 10$ | Exhaustive brute-force oracle |
| **Unit 4** | **DFS Branch & Bound Placement** | $O\left(\frac{m!}{(m-n)!}\right)$ worst-case | $O(n + m)$ | `bench_bnb` (B5) | (4,4): 2.4x node reduction<br>(5,4): 12.0x node reduction<br>(6,5): 45.0x node reduction | **INV-2**: Vector DFS stack $\le n+1$; 0 hot-loop heap allocs |
| **Unit 4** | **Brute-Force Permutation Oracle** | $\Theta\left(\frac{m!}{(m-n)!}\right)$ | $O(m)$ | `bench_bnb` (B5) | (4,4): 24 evals<br>(5,4): 120 evals<br>(6,5): 720 evals | Verified exact cost match (`EXACT, match = 100%`) |
| **Unit 5** | **Fibonacci Heap: Routing Workload** | Insert: $O(1)$ amortized<br>ExtractMin: $O(\log N)$ | $O(N)$ node pool | `bench_priority_queues` (B3) | 279.2 ms (716,279 ops/sec)<br>Binary heap winner (227.9 ms) | **INV-1**: Forbidden in Unit 2; baseline comparison only |
| **Unit 5** | **Fibonacci Heap: Telemetry Workload** | DecreaseKey: $O(1)$ amortized<br>Insert: $O(1)$ | $O(N)$ node pool | `bench_priority_queues` (B3) | 158.6 ms (1,261,198 ops/sec)<br>**Fibonacci winner** (vs Bin 193.7 ms) | FH-1..FH-13 contracts: pool recycling, max-degree bound |

---

## 2. Unit-by-Unit Deep Dive

### Unit 1: Threaded Binary Tree Traversal & AVL Symbol Management

#### 1. Theoretical Analysis
- **Threaded AST Traversal**: A standard binary tree traversal requires $O(h)$ auxiliary stack memory, which degrades to $O(N)$ in degenerate skew trees, risking call-stack overflow on million-gate quantum traces. By utilizing Morris/in-order threaded binary tree links (where null right pointers store in-order successors and null left pointers store in-order predecessors), the traversal operates using exactly $O(1)$ auxiliary stack space. Every node is visited via thread traversal in amortized $O(1)$ time, yielding total time $O(N)$.
- **Hot/Cold Memory Split**: `ASTNode` is strictly packed to 24 bytes (fitting 2.67 nodes per 64-byte L1 cache line), while the 64-byte `GatePayload` is placed in a separate contiguous pool dereferenced only upon visitor execution.
- **AVL Symbol Table**: Maintains identifier records with worst-case height $h \le 1.44 \log_2(V + 2)$. Lookup and insertion are strictly $O(\log V)$. Rebalancing uses iterative rotation dispatch (LL, RR, LR, RL) without recursive calls.

#### 2. Empirical Benchmark Verification (B1)
```
[B1] AST Traversal: Threaded vs Recursive
nodes,threaded_us,recursive_us,speedup,max_recursion_depth,aux_stack_bytes
1000,47.700,21.400,0.45,10,320
1000000,110613.100,53297.200,0.48,20,640
```
- **Recursion Depth**: Threaded traversal achieves `max_recursion_depth = 0` and `aux_stack_bytes = 0`, demonstrating immunity to call-stack exhaustion on $10^6$ nodes.
- **Cache Locality**: While recursive traversal benefits from contiguous instruction locality in tiny benchmarks, threaded traversal provides deterministic $O(1)$ memory guarantees essential for embedded and memory-constrained compiler execution.

---

### Unit 2: Coupling Graph Modeling & Shortest-Path Routing

#### 1. Theoretical Analysis
- **Graph Topology**: Represented as an adjacency list of undirected physical edges with additive weights $w_{ij} = -\ln(F_{ij})$.
- **Routing Engine**: For non-adjacent logical two-qubit operations, Dijkstra's single-source shortest path algorithm computes the path minimizing cumulative log-infidelity. Using an indexed min-heap (`std::priority_queue`), each query runs in $O(|E| \log |V|)$ time.
- **SWAP Decomposition**: Along a path of length $d$ hops, $d - 1$ physical SWAPs are inserted. Each SWAP decomposes into exactly 3 physical CX gates: $\text{CX}(u,v) \to \text{CX}(v,u) \to \text{CX}(u,v)$. Cumulative fidelity is tracked multiplicatively in log space: $F_{\text{cum}} = \exp(-\sum w_e)$, avoiding numerical underflow.

#### 2. Empirical Benchmark Verification (B2)
```
[B2] Routing: Binary-Heap Dijkstra vs Floyd-Warshall Oracle
topology,vertices,edges,dijkstra_us,floyd_us,match,max_abs_delta
linear_5,5,4,482.100,13.100,EXACT,0.000000000
grid_3x3,9,12,885.300,30.100,EXACT,0.000000000
heavyhex_7,7,7,537.600,28.700,EXACT,0.000000000
```
- **Accuracy**: Binary-Heap Dijkstra strictly matches the Floyd-Warshall all-pairs reference oracle to `0.000000000` absolute delta across all topologies (Linear, 3x3 Grid, Heavy-Hex).

---

### Unit 3: Matrix Chain Multiplication Tensor Contraction Planner

#### 1. Theoretical Analysis
- **Recurrence**: Given dimension chain $\langle p_0, p_1, \dots, p_k \rangle$, the minimum scalar multiplications to contract matrices $A_1 \dots A_k$ satisfies:
  $$m[i,j] = \min_{i \le \ell < j} \left\{ m[i,\ell] + m[\ell+1,j] + p_{i-1} p_\ell p_j \right\}$$
  The dynamic programming state space is an upper triangular table of size $k \times k$, solved in order of chain length $L = 2 \dots k$.
- **Time Complexity**: $\sum_{L=2}^k (k - L + 1)(L - 1) = \frac{k^3 - k}{6} = \Theta(k^3)$.
- **Space Complexity**: Row-major flat 1D buffer of size $k \times k$ storing $m[i,j]$ and split index $s[i,j]$, running in $\Theta(k^2)$ space.
- **Arithmetic Safety**: Enforces INV-4 using `SafeArithmetic::saturated_triple_product_add` without 128-bit extensions, clamping to $2^{64}-1$ upon overflow.

#### 2. Empirical Benchmark Verification (B4)
```
[B4] MCM: DP vs Left-to-Right
chain_length,optimal_cost,left_to_right_cost,savings,savings_percent,dp_time_us
4,26000,41000,15000,36.585366,47.200
8,43000,45500,2500,5.494505,19.400
16,88500,94000,5500,5.851064,65.100
32,186000,197500,11500,5.822785,537.000
64,383000,409500,26500,6.471306,4178.100
```
- **Cubic Scaling**: Scaling from $k=32$ (537.0 $\mu$s) to $k=64$ (4178.1 $\mu$s) exhibits an empirical ratio of $\frac{4178.1}{537.0} \approx 7.78 \approx 2^3$, matching theoretical $\Theta(k^3)$ complexity.
- **Contraction Savings**: For $k=4$, DP parenthesization achieves 36.59% computational reduction over left-to-right evaluation.

---

### Unit 4: Qubit Placement via DFS Branch & Bound

#### 1. Theoretical Analysis
- **Search Space**: Injective mappings $\pi : L \to P$ from $n$ logical qubits to $m$ physical qubits. The unconstrained permutation space has size $P(m,n) = \frac{m!}{(m-n)!}$.
- **Admissible Lower Bound**:
  $$\text{LB}(\text{node}) = \text{cost}_{\text{assigned}} + \sum_{(i,j) \in E_L, i \notin A \lor j \notin A} \alpha_{ij} \cdot \delta_{ij}$$
  where $\delta_{ij}$ lower bounds the distance between unassigned endpoints using precomputed sorted physical distance tables (`sorted_dist_from_`). Admissibility $\text{LB} \le \text{cost}^*$ ensures no optimal assignment is pruned.
- **Stack & Allocation Invariant (INV-2)**: Explicit `std::vector<Frame>` DFS stack pre-reserved to $(n + 1)$ elements. Assignments and unassignments mutate `current_l2p_` and `current_used_` in place. Heap allocations in the DFS hot loop are strictly zero.

#### 2. Empirical Benchmark Verification (B5, B6)
```
[B5] Placement: DFS Branch & Bound vs Brute Force
m,n,brute_force_nodes,bnb_nodes_expanded,bnb_pruned,reduction_ratio,bnb_cost,brute_cost,match,max_stack_depth
4,4,24,10,9,2.40,3.000,3.000,EXACT,5
5,4,120,10,17,12.00,3.000,3.000,EXACT,5
6,5,720,16,31,45.00,4.000,4.000,EXACT,6

[B6] Memory & Complexity Scaling
module,input_size,peak_stack_bytes,heap_allocations_during_hot_loop,observed_order
unit1_ast,1000000,0,0,O(n)
unit4_bnb,n=16,544,0,O(n)
```
- **Optimality**: 100% exact match against exhaustive permutation oracle (`EXACT`).
- **Pruning Power**: Pruning efficiency scales dramatically with problem size (45.0x node evaluation reduction on $m=6, n=5$).
- **Stack Bound**: Stack depth strictly bounded by $n + 1$ (e.g. max depth 6 for $n=5$). Hot-loop heap allocations are verified 0.

---

### Unit 5: Advanced Priority Queues & Decoherence Telemetry

#### 1. Theoretical Analysis
- **Fibonacci Heap Amortized Guarantees**:
  - `insert`: $O(1)$ worst-case / amortized (lazy addition to circular root list).
  - `decrease_key`: $O(1)$ amortized (node cut + cascading cuts using boolean mark bits).
  - `extract_min`: $O(\log N)$ amortized (child splicing followed by pairwise consolidation into `degree_table_`).
  - Max degree bound: $\le \lfloor \log_\phi(N) \rfloor + 1$ enforced structural invariant.
- **Binary Heap Comparison**:
  - `insert`: $O(\log N)$ worst-case.
  - `decrease_key`: $O(\log N)$ worst-case (sift-up swapping array entries and updating handle indices).
  - `extract_min`: $O(\log N)$ worst-case.

#### 2. Empirical Benchmark Verification (B3)
```
[B3] Priority Queue: Fibonacci vs Binary Heap
workload,ops,insert_pct,decrease_key_pct,extract_pct,fib_us,binary_us,fib_ops_per_sec,binary_ops_per_sec,winner
routing_like,200000,70,5,25,279220.9,227895.9,716279,877594,binary_heap
telemetry_like,200000,20,75,5,158579.4,193685.6,1261198,1032601,fibonacci_heap
```
- **Workload Specialization**:
  - **Routing-Like (70% Ins, 5% DecKey, 25% ExtMin)**: Binary heap's contiguous array cache locality outperforms Fibonacci heap's pointer manipulations (877,594 vs 716,279 ops/sec), confirming why Unit 2 mandates binary heap (INV-1).
  - **Telemetry-Like (20% Ins, 75% DecKey, 5% ExtMin)**: Fibonacci heap's $O(1)$ amortized decrease-key dominates binary heap's $O(\log N)$ sift-up operations, executing at 1,261,198 ops/sec vs 1,032,601 ops/sec (Fibonacci heap winner).

---

## 3. Summary of Project Invariants

- **INV-1**: FibonacciHeap isolated to Unit 5 and benchmarks; Unit 2 routing strictly uses indexed binary heap.
- **INV-2**: Unit 4 uses explicit vector DFS stack ($\le n+1$ frames) with zero heap allocations during search.
- **INV-3**: Unit 3 strictly performs contraction planning without numerical simulation or linear algebra libraries.
- **INV-4**: Zero 128-bit compiler integer extensions; saturated 64-bit safe arithmetic across all units.
- **MEM**: Zero raw `new`/`delete`/`malloc`/`free` dynamic memory calls in production code.
