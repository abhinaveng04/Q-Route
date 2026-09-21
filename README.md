# Q-Route: Quantum Neural Network (QNN) Routing Engine

Q-Route is a high-performance, deterministic compiler backend for mapping and routing Quantum Neural Network (QNN) quantum circuits onto constrained physical quantum processing unit (QPU) topologies.

## Core Features & DSA-II Modules

1. **Unit 1 (Trees - Ingestion Layer)**: Cache-conscious OpenQASM 2.0 lexer and parser, threaded binary search tree with O(1) auxiliary-space in-order traversal, and balanced AVL symbol table.
2. **Unit 2 (Graphs - Network Optimization)**: Physical coupling graph representation, Dijkstra shortest-path routing with non-negative log-fidelity weights (-ln F) using standard binary heaps, and 3xCX SWAP decomposition synthesizer.
3. **Unit 3 (Dynamic Programming - MCM Planning)**: Matrix Chain Multiplication dynamic programming optimizer for computing minimal-FLOP unitary contraction schedules during classical verification.
4. **Unit 4 (Branch & Bound - Qubit Placement)**: Depth-First Search Branch & Bound mapper computing initial logical-to-physical qubit layouts with admissible distance lower bounding.
5. **Unit 5 (Advanced Data Structures - Telemetry)**: Cache-friendly Fibonacci Heap supporting O(1) amortized insert and decrease-key operations for real-time decoherence hazard monitoring.

## Build Requirements

- C++17 compliant compiler (GCC >= 9, Clang >= 10, or MSVC >= 19.20)
- CMake >= 3.16
- Python >= 3.8

## Quick Start

`ash
cmake -S . -B build -DQRoute_BUILD_TESTS=ON -DQRoute_BUILD_BENCHMARKS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
python3 scripts/check_invariants.py
`
