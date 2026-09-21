Program and Branch Name: Bachelor of Technology in Computer Science & Engineering (Artificial Intelligence & Machine Learning)  
PBL Progress Report – I

| Field | Details |
| --- | --- |
| Course Name | Data Structures and Algorithms - II |
| Course Code | BCSE0401 |
| Faculty Name | [Enter Faculty / Course Coordinator Name] |
| Assignment Type | Individual Assignment |
| Student(s) Name | Abhinav Gupta |
| ERP ID (I Year) / Roll No. (II Year onwards) | [Enter your University Roll No.] |
| Team/Group No. (Group/Capstone only) | N/A – Individual Assignment |
| Problem Statement / Project Title | Q-Route: Quantum Neural Network (QNN) Routing Engine |
| SDG(s) | SDG 9 – Industry, Innovation & Infrastructure |
| Reporting Month | Month 1 |
| Date of Submission | September 21, 2026 |


Student Declaration

I certify that this assignment is my own work, with all sources appropriately acknowledged. I understand that a 0% mark and further academic penalties may apply if the content is found to be plagiarised or inappropriately AI-generated.

Student Name & Signature: Abhinav Gupta  
Date: September 21, 2026


PBL Progress Report – I  
Review 1: Problem Understanding & Initial Progress  

Review Coverage:  
1. Individual / Group Assignment: Unit/Module 1 and Unit/Module 2  


1. Problem Statement / Assignment or Project Title

My project title is “Q-Route: Quantum Neural Network (QNN) Routing Engine.”

Q-Route is a C++17 compiler backend and simulator intended for Quantum Neural Networks. Its main purpose is to help map high-level QNN circuits onto real quantum hardware in a smarter and more efficient way. Many quantum processors do not allow every pair of qubits to interact directly. Because of this, a compiler must decide where to place logical qubits on physical qubits, how to insert SWAP operations when two qubits need to interact but are not adjacent, and how to plan computation so that the final circuit remains as shallow and reliable as possible.

In simple terms, Q-Route tries to solve the routing and placement problem for quantum circuits. It focuses on reducing unnecessary gate overhead, improving memory behaviour during compilation, and producing valid executable sequences for connectivity-constrained Quantum Processing Units (QPUs).


2. Problem Context & Background

Quantum Neural Networks are variational quantum models that use parameterized quantum gates to represent learnable weights and activations. Common single-qubit gates such as Rx, Ry, Rz, and Hadamard are used to encode and transform quantum states, while two-qubit gates such as Controlled-NOT (CX or CNOT) create entanglement between qubits. This entanglement is essential for quantum machine learning models because it allows information to be shared across multiple qubits.

However, present-day quantum hardware belongs to the Noisy Intermediate-Scale Quantum (NISQ) era. Devices from platforms such as IBM and Rigetti usually have limited connectivity. Two-qubit gates can often be applied only between physically adjacent qubits. These adjacency restrictions are represented by a coupling graph, which may be a linear chain, a 2D grid, or a heavy-hex lattice depending on the architecture.

When a QNN circuit requires an operation between two logical qubits that are not currently adjacent on the physical device, the compiler must insert SWAP gates to move qubit states closer together. This creates a serious problem because one SWAP gate is usually decomposed into three CNOT gates. Since CNOT gates are noisy and contribute significantly to circuit depth, poor SWAP routing quickly increases the total error probability and causes the quantum state to decohere. Therefore, an efficient routing engine is important for making QNN execution more practical on real hardware.


3. Problem Identification / Understanding

During the first month, I identified four major problems that Q-Route needs to handle.

The first problem is classical AST traversal overhead. A quantum circuit can be represented as an Abstract Syntax Tree after parsing. Traditional recursive traversal methods use call-stack memory proportional to tree height. For large circuits with many operations, this can become inefficient and may even cause stack overflow. I therefore needed a traversal method that avoids recursion and uses constant auxiliary space where possible.

The second problem is cache inefficiency in compiler passes. Many compiler representations use large object-oriented nodes with virtual functions and scattered memory layouts. This can cause poor CPU cache behaviour when the compiler repeatedly traverses the same structure. To address this, I separated lightweight traversal metadata from heavier semantic payload data so that the tree traversal remains more cache-friendly.

The third problem is topological hardware limitation. Since physical qubits are connected only according to the coupling graph, the compiler must find valid paths between qubits before applying two-qubit gates. The chosen path should not only be short in terms of number of edges, but also should preserve high overall gate fidelity.

The fourth problem is metric domain mismatch. Quantum gate fidelities are multiplicative in nature. If a path contains several gates, the total path fidelity is the product of individual edge fidelities. Standard shortest-path algorithms such as Dijkstra work on additive non-negative weights. To make the problem compatible with shortest-path routing, I converted multiplicative fidelity values into additive logarithmic costs using the transformation:

w_ij = -ln(F_ij)

Since fidelity F_ij is between 0 and 1, the transformed weight is non-negative and can be used safely in shortest-path algorithms.


4. Objectives

The overall objective of this assignment is to build a clean, reliable, and well-tested C++17 compiler infrastructure for QNN routing, while applying the data structure concepts studied in Unit 1 and Unit 2.

For Unit 1, my objectives were to design tree-based structures that support efficient parsing, traversal, and symbol management. I aimed to implement a cache-conscious separation between AST traversal nodes and gate payload data. I also implemented a two-way Threaded Binary Tree to support non-recursive in-order traversal with O(1) auxiliary space. In addition, I built an AVL Tree-based symbol table to provide balanced O(log n) identifier lookup for declared quantum registers.

For Unit 2, my objectives were to model quantum hardware connectivity as a weighted graph and implement routing algorithms that respect physical constraints. I represented the coupling map using a compact adjacency-list structure. I converted fidelity values into additive logarithmic costs so that shortest-path algorithms could be applied correctly. I then implemented Dijkstra’s algorithm using std::priority_queue with deterministic tie-breaking and lazy deletion. I also maintained a dynamic placement map to track the correspondence between logical and physical qubits, and built a SWAP synthesizer that converts a selected path into valid sequences of three-CNOT SWAP operations.

Beyond these unit-specific goals, I also aimed to follow disciplined software engineering practices. The codebase uses RAII memory management, avoids raw owning pointers, maintains strict compiler warning settings, and includes automated invariant checks and unit tests.


5. Target Users / Stakeholders

The main target users of this project are researchers and developers working in quantum machine learning and quantum compiler design.

Quantum Machine Learning Researchers can use Q-Route as a study tool for understanding how QNN circuits behave when compiled for restricted hardware. They can observe how placement and routing decisions affect circuit depth, gate count, and expected noise.

Quantum Software and Compiler Engineers can benefit from the modular design of the project. Although Q-Route is not a full production compiler, it demonstrates useful ideas for intermediate representation handling, graph-based routing, and memory-efficient AST design. These ideas are relevant to larger quantum toolchains such as Qiskit, Cirq, and TKET.

HPC and Quantum Cloud Providers may also be interested in this type of work because better routing can improve the success probability of quantum jobs executed on noisy hardware. Even small reductions in unnecessary SWAP gates can lead to more reliable results and better utilization of limited quantum processing resources.


6. Research / Literature Review Completed

During Month 1, I reviewed literature and reference material related to quantum circuit representation, tree data structures, graph algorithms, and NISQ hardware constraints.

I studied the OpenQASM 2.0 language specification to understand how quantum circuits are commonly represented in text form. This helped me design a lexer and recursive-descent parser capable of reading basic circuit descriptions, handling register declarations, and performing simple compile-time constant folding.

I reviewed the concept of Threaded Binary Trees, especially the idea of reusing null child pointers to store in-order predecessor and successor links. This was important for implementing non-recursive traversal without relying on an explicit stack.

I also studied AVL trees and their balancing properties. The theoretical height bound of an AVL tree gave me confidence that symbol table lookups would remain logarithmic even when identifiers are inserted in sorted order. I reviewed the four standard rotation cases: LL, RR, LR, and RL.

For graph modelling and routing, I revised Dijkstra’s shortest-path algorithm, priority-queue implementations, and the importance of deterministic tie-breaking. I also looked at Breadth-First Search for hop-distance analysis, Iterative Depth-First Search for graph exploration, and minimum spanning tree algorithms such as Prim’s and Kruskal’s. For validation purposes, I studied Floyd-Warshall and Bellman-Ford as reference oracles for checking shortest-path results.

Finally, I examined common NISQ coupling topologies, including IBM-style linear chains, small 2D grids, and heavy-hex architectures. This helped me create realistic test graphs for routing experiments.


7. Unit/Module Concepts Identified for Application

Unit 1: Tree-Based Data Management

The Unit 1 concepts applied in this project include Threaded Binary Trees, AVL Trees, Binary Search Trees, and memory-aware node design.

I used a two-way Threaded Binary Tree to represent the parsed AST in a way that supports in-order traversal without recursion. This directly applies the idea of using null pointers as threading links.

I used an AVL Tree as the symbol table for register and identifier resolution. This applies self-balancing search tree concepts and ensures predictable logarithmic lookup performance.

I also compared ordinary BST behaviour with balanced AVL behaviour, especially for sorted insertion patterns, to understand why balancing is necessary in compiler symbol tables.

Unit 2: Graph Modeling & Network Optimization

The Unit 2 concepts applied in this project include weighted adjacency lists, shortest-path algorithms, graph traversal, and minimum spanning trees.

I modelled the quantum processor connectivity as a weighted directed or undirected graph depending on the coupling map. Each edge stores a physical fidelity value and its transformed additive cost.

I implemented Dijkstra’s algorithm for minimum-cost routing between physical qubits. This is the central graph algorithm used for SWAP path selection.

I also used BFS for hop-distance measurement, iterative DFS for graph exploration, and Prim’s and Kruskal’s algorithms for connectivity analysis. Floyd-Warshall and Bellman-Ford were implemented as validation tools to confirm that the Dijkstra results are correct.


8. Work Completed During Month 1

During the first month, I completed the initial project scaffold and the core implementation required for Review 1.

First, I set up the build system using CMake 3.16 or newer. The project enforces C++17, disables compiler extensions, and uses strict warning flags such as -Wall, -Wextra, and -Wpedantic. This helped catch potential bugs early and encouraged cleaner code from the beginning.

I also implemented common utility components. These include safe 64-bit arithmetic with saturation behaviour, a simple file reader for loading circuit and coupling files, FNV-1a hashing for identifier handling, and lightweight timing utilities for future performance measurement.

For Unit 1, I implemented the AST memory model. I separated the traversal node ASTNode from the heavier GatePayload structure. The sizes were fixed using compile-time static assertions: ASTNode is 24 bytes and GatePayload is 64 bytes. This separation helps keep tree traversal more cache-friendly.

I then implemented the ThreadedAST structure. The tree is built from the parsed circuit representation and supports in-order traversal using threaded links rather than recursion. I also implemented the AVL symbol table with RAII-based ownership and support for all rotation cases. This allowed the parser to resolve declared registers and identifiers efficiently.

In addition, I built a basic OpenQASM 2.0 lexer and recursive-descent parser. The parser can read simple circuit files, recognize gate statements, handle register declarations, and perform basic constant arithmetic folding where applicable.

For Unit 2, I implemented the physical coupling graph representation. The PhysicalEdge structure is packed to 32 bytes and stores endpoint information, fidelity, transformed weight, and related metadata. The PhysicalCouplingGraph class loads topology files such as linear-chain maps, 9-qubit grid maps, and 7-qubit heavy-hex maps.

I implemented the logarithmic weight transformation so that multiplicative fidelities become additive non-negative costs. After validating that all transformed weights satisfy w >= 0, I implemented Dijkstra’s algorithm using std::priority_queue. The priority queue uses deterministic tie-breaking based first on cost and then on vertex identifier. Lazy deletion is used to ignore stale queue entries.

I also implemented the PlacementMap class to maintain a bijective mapping between logical qubits and physical qubits. Whenever a SWAP is performed, the mapping is updated carefully to preserve validity. The SwapSynthesizer converts a selected physical path into a sequence of legal SWAP operations, each decomposed into three CNOT gates.

Finally, I implemented supporting graph algorithms for testing and validation, including BFS, iterative DFS, Prim’s MST, Kruskal’s MST, Floyd-Warshall, and Bellman-Ford. These were not all part of the final routing pipeline, but they helped verify correctness and explore alternative graph-based approaches.

By the end of Month 1, the project passed all planned unit tests and invariant checks. The implementation covered the required Unit 1 and Unit 2 topics and produced a stable foundation for the next review stage.


9. Evidence / Supporting Material

The main evidence for Month 1 progress is the source code itself, along with test outputs and validation scripts.

The repository contains the full implementation under include/qroute/ and src/, divided into common utilities, Unit 1 AST components, and Unit 2 graph components. The code is organised so that each module can be tested independently.

I verified the memory layout contracts using compile-time static assertions. The confirmed sizes are:

- sizeof(ASTNode) == 24 bytes
- sizeof(GatePayload) == 64 bytes
- sizeof(PhysicalEdge) == 32 bytes

These checks ensure that the cache-conscious design assumptions are actually enforced by the compiler.

I also ran the unit test suite. A total of 33 test assertions passed across three groups:

- common_safe_arithmetic: 8/8 tests passed  
  These checked saturation behaviour, boundary arithmetic, and hashing utilities.

- unit1_ast: 11/11 tests passed  
  These checked traversal order, non-recursive behaviour, AVL balance, and symbol table lookup. One important test inserted 10,000 sorted identifiers and confirmed that AVL height remained within the expected logarithmic bound.

- unit2_graph: 14/14 tests passed  
  These checked Dijkstra routing against Floyd-Warshall and Bellman-Ford results, compared Prim and Kruskal MST total weights, verified edge legality, checked placement bijection invariants, and confirmed that SWAP synthesis produces valid three-CNOT sequences.

I also created an automated invariant scanner named scripts/check_invariants.py. It checks for forbidden patterns, verifies basic structural assumptions, and reports zero violations for the current build. One specific check confirmed that no prohibited Fibonacci-heap usage appears in the Unit 2 implementation, keeping the project aligned with the intended binary-heap Dijkstra approach.

A sample CLI execution was also performed. Running the executable on a small circuit file produced expected output, including the correct number of AST nodes and symbol table entries. This showed that the parser, tree construction, and symbol table integration are working together properly.


10. Challenges Faced

One of the first challenges was achieving the exact memory layouts required for cache-conscious design. Getting ASTNode to fit into 24 bytes, GatePayload into 64 bytes, and PhysicalEdge into 32 bytes required careful ordering of fields, bit-flags, indices, and padding. I had to avoid unnecessary alignment overhead while still keeping the structures readable and maintainable.

Another challenge was implementing non-recursive threaded traversal safely. Converting null child pointers into predecessor and successor threads is straightforward in theory, but easy to get wrong in practice. I had to ensure that no dangling links were left behind after tree construction. I solved this by performing a controlled in-order finalization pass and releasing temporary buffers immediately after use.

Dijkstra’s algorithm also introduced a subtle issue due to floating-point costs. When two paths had very similar costs, different platforms or compiler optimisations could lead to slightly different vertex exploration orders. To make the behaviour deterministic, I added explicit tie-breaking rules and used an epsilon-based comparison for stale priority-queue entries.

The most conceptually difficult part was preserving the bijection between logical and physical qubits during SWAP synthesis. A naive swap implementation could accidentally map two logical qubits to the same physical qubit or leave one physical qubit unmapped. I resolved this by maintaining forward and reverse lookup tables inside PlacementMap and updating both sides atomically whenever a physical swap occurs.


11. Individual / Team Contribution

This is an individual assignment, so the contribution is 100% from me, Abhinav Gupta.

I formulated the project idea, defined the scope for Review 1, designed the overall architecture, and implemented the CMake build system. I wrote the Unit 1 components, including the AST node layout, threaded tree traversal, AVL symbol table, lexer, and parser. I also implemented the Unit 2 components, including the coupling graph, logarithmic weight transformation, Dijkstra routing, placement map, and SWAP synthesizer.

In addition, I authored the unit tests, created the invariant-checking script, and performed the validation runs reported in this progress update.


12. Overall Progress (%)

My overall progress for Review 1 is approximately 45%.

This percentage reflects the completion of the foundational work required for Month 1. The build system, common utilities, Unit 1 tree structures, Unit 2 graph structures, routing logic, and initial validation framework are all implemented and tested. At this stage, the project already satisfies the main expectations of Review 1.

The remaining work involves deeper optimisation and expansion into later units, especially branch-and-bound placement and dynamic-programming-based contraction planning. Those parts are important for the final project outcome, but they were not required for the first review.


13. Plan for Next Review

For Review 2, I plan to move beyond the basic routing engine and start implementing the optimisation layers that will make Q-Route more complete.

The next major focus will be Unit 4, specifically branch-and-bound based qubit placement. I intend to build a lookahead interaction graph from the parsed circuit so that frequently interacting logical qubits can be identified early. Then I will implement a depth-first search branch-and-bound procedure for assigning logical qubits to physical qubits. The search will be initialised with a greedy upper bound to prune poor branches earlier. I will also design an admissible lower-bound function that estimates the minimum additional routing cost from the current partial placement. The implementation will track assignment state in-place to keep stack depth manageable.

Alongside that, I plan to begin Unit 3 work on dynamic programming for tensor contraction planning. The first target will be a matrix-chain-multiplication style DP formulation using flattened one-dimensional tables for m[i][j] and s[i][j]. This will allow me to compute an optimal contraction order and reconstruct the corresponding parenthesization. I also want to measure the estimated floating-point operation savings compared with a naive left-to-right contraction strategy.

For validation, I plan to cross-check the branch-and-bound placement results against brute-force permutation search on small instances. Similarly, the DP contraction planner will be tested against exhaustive parenthesization enumeration for tiny chain lengths. These comparisons will help confirm correctness before scaling to larger inputs.

By the end of Review 2, I expect to have a working prototype that combines initial parsing, graph routing, placement search, and contraction planning, with tests covering the newly added units.


Faculty Evaluation:

To be completed by Faculty in Moodle