PRD.md — Q-Route: Quantum Neural Network (QNN) Routing Engine

Document Type: Machine-Actionable Product Requirements Document (PRD)
Authoritative Role: Single source of truth for the agy autonomous coding CLI
Revision: R2 (Binding Architectural Fixes Incorporated)
Language Standard: C++17 · Build: CMake ≥ 3.16 · Tests: GoogleTest (with native-assertion fallback)

AGENT EXECUTION CONTRACT

agent_contract:
  project_name: "Q-Route"
  projectfulltitle: "Quantum Neural Network (QNN) Routing Engine"
  course: "BCSE0401: Data Structures and Algorithms - II"
  institution: "NIET Greater Noida"
  sdg_alignment: "SDG 9: Industry, Innovation & Infrastructure"
  domain: "Artificial Intelligence + High-Performance Computing (HPC) Infrastructure"
  language_standard: "C++17"
  cmake_minimum: "3.16"
  testframework: "GoogleTest via FetchContent; fall back to qroute/tests/nativeassert.hpp if fetch is unavailable"
  supported_compilers: ["GCC >= 9", "Clang >= 10", "MSVC >= 19.20 (VS2019 16.8+)"]
  warningpolicy: "-Wall -Wextra -Wpedantic; /W4 on MSVC; warnings-as-errors in CI mode (-DQRouteWERROR=ON)"
  determinism: "MANDATORY. Identical inputs + identical flags => byte-identical stdout report (excluding wall-clock timing fields)."

execution_protocol:
  order: "Execute Section E tasks strictly sequentially: Task 1 -> Task 2 -> Task 3 -> Task 4 -> Task 5 -> Task 6."
  looppertask:
    - "1. Read the task manifest (files, steps, tests, acceptance)."
    - "2. Create/modify ONLY the files listed (plus new headers/sources consistent with Section C)."
    - "3. Configure + build."
    - "4. Run the full test suite (all previously completed tasks' tests must still pass)."
    - "5. Run the invariant scanner (Section 0.3)."
    - "6. If ANY test fails, ANY warning-as-error triggers, or ANY invariant fails: fix and re-run before proceeding."
    - "7. Emit the task completion manifest (Section 0.4)."
  hard_stop: "Never begin Task N+1 while Task N gate is FAILED."

definitionofdone_global:
  - "Zero compiler warnings under the warning policy."
  - "Zero failing tests."
  - "Zero invariant violations (Section 0.3)."
  - "All static_assert size contracts hold (Section B)."
  - "qroute --circuit data/circuit.qasm --coupling data/coupling.map --out build/routed.qasm exits 0."
  - "qroute --bench completes and prints the benchmark schema (Section D.5)."

0.1 Non-Negotiable Architectural Invariants (R2 Binding Fixes)

These four rules override every other instruction in this PRD and every heuristic the agent may infer.

| ID | Invariant | Enforcement |
|---|---|---|
| INV-1 | Unit 2 (Graphs) routing MUST use std::priorityqueue (binary heap) exclusively. The custom FibonacciHeap is strictly forbidden in any Unit 2 source/header. Deterministic tie-breaking via std::pair + std::greater<>. | scripts/check_invariants.py grep gate; code review checklist |
| INV-2 | Unit 4 (Placement) MUST use Depth-First Search Branch & Bound with an explicit std::vector stack and a global upper-bound register B. No best-first open list, no std::priorityqueue, no FibonacciHeap in Unit 4. Stack depth ≤ n; working memory O(n + m). | grep gate; runtime assertion maxstack_depth()  @ :.

RULES = [
  # (ruleid, pathstoscan, forbiddenregex, description)
  ("INV-4", ["include","src","apps","tests","benchmarks"],
   r"(int128|umul128|uint128t)", "128-bit compiler extension integer"),

  ("INV-1", ["include/qroute/unit2graph","src/unit2graph"],
   r"FibonacciHeap", "Fibonacci heap forbidden in Unit 2 routing"),

  ("INV-2", ["include/qroute/unit4bnb","src/unit4bnb"],
   r"(std::priority_queue|FibonacciHeap|std::multiset)", "best-first open list forbidden in Unit 4"),

  ("INV-3", ["include/qroute/unit3dp","src/unit3dp"],
   r"(std::complex|blas|lapack|Eigen|state_vector|amplitude)", "numerical simulation forbidden in Unit 3"),

  ("MEM",   ["include","src","apps"],
   r"(^|[^_[:alnum:]])(new|delete|malloc|free)[[:space:]]*[\(\[]", "raw dynamic memory management"),
]

Gate: python3 scripts/check_invariants.py must print ALL INVARIANTS SATISFIED and exit 0.

0.4 Per-Task Completion Manifest

After each task, agy must emit:

task_completion:
  id: "TASK-"
  status: "COMPLETE | FAILED"
  files_created: [ ... ]
  files_modified: [ ... ]
  build: { configured: true, compiled: true, warnings: 0 }
  tests: { total: , passed: , failed: 0 }
  invariants: { scannerexitcode: 0 }
  gate: "PASS | FAIL"
  notes: ""

A. System Overview & Problem Statement

A.1 Domain Context

Quantum Neural Networks (QNNs) are parameterized quantum circuits in which:

Single-qubit rotations (rx, ry, rz, h) encode AI weight transformations and activations.
Two-qubit entangling gates (cx) encode tensor-parameter interactions between logical neurons.

Physical NISQ quantum processing units (QPUs) do not provide all-to-all connectivity. Physical qubits are embedded in a restricted coupling graph (linear chain, 2D grid, or heavy-hex lattice). A two-qubit gate may execute only between physical qubits sharing a hardware edge.

A.2 The Routing Problem

When a QNN trace requests entanglement between logical qubits mapped to non-adjacent physical qubits, the compiler backend must insert intermediate SWAP operations.

SWAP decomposition cost (hard requirement):

SWAP(a,b) = CX(a,b) · CX(b,a) · CX(a,b)

\[
1 \times SWAP \equiv 3 \times CX
\]

Unoptimized SWAP insertion causes:

Circuit depth inflation.
Accelerated decoherence (\(T1\) relaxation, \(T2\) dephasing).
Cumulative gate infidelity.
Collapse of state fidelity \(|\psi\rangle\) before circuit completion.

A.3 Fidelity-to-Weight Transformation (Core Mathematical Requirement)

Each physical edge \((i,j)\) carries a two-qubit gate fidelity:

\[
F_{ij} \in (0, 1]
\]

Compound fidelity along a path \(P = v0, v1, \dots, v_d\) is multiplicative:

\[
F(P) = \prod{r=0}^{d-1} F{vr v{r+1}}
\]

Q-Route converts this into an additive metric so that Dijkstra's algorithm applies:

\[
\boxed{\,w{ij} = -\ln(F{ij})\,}
\]

Since \(F{ij} \le 1 \Rightarrow w{ij} \ge 0\), Dijkstra's non-negative weight precondition is satisfied, and:

\[
W(P) = \sum{r=0}^{d-1} w{vr v{r+1}} = -\ln F(P)
\]

Therefore minimizing \(W(P)\) is mathematically equivalent to maximizing \(F(P)\).

Validation rules:

| Condition | Behavior |
|---|---|
| \(F_{ij} \in (0,1]\) | Accept; weight = -log(F) |
| \(F{ij} > 1\) | Reject: std::invalidargument("fidelity > 1") |
| \(F{ij} \le 0\) | Reject: std::invalidargument("fidelity must be in (0,1]") |
| \(F_{ij} = 1\) | Accept; weight = 0.0 (perfect link) |
| Self-loop u == v | Reject |
| Duplicate edge | Reject (deterministic topology requirement) |

A.4 End-to-End Pipeline

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
        |  explicit std::vector stack, global upper bound B                   |
        |  min  sum{(i,j) in Glookahead} alpha_ij * dist(pi(i), pi(j))             |
        +-------------------------------------+--------------------------------------+
                                              |  initial mapping pi : L -> P
                                              v
        +-------------------------------------+--------------------------------------+
        |  Unit 2: Routing Engine  [INV-1]                                           |
        |  PhysicalCouplingGraph (adjacency list, wij = -ln Fij)                   |
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
        |  O(1) amortized insert / decreasekey, O(log n) amortized extractmin      |
        +-------------------------------------+--------------------------------------+
                                              |
                                              v
        +-------------------------------------+--------------------------------------+
        |  Outputs: routed physical circuit, deterministic report, MCM plan,         |
        |           decoherence alert log, CPU timing telemetry, benchmark tables    |
        +----------------------------------------------------------------------------+

A.5 Success Criteria (Product Level)

| ID | Criterion | Measurement |
|---|---|---|
| SC-1 | All five DSA-II units implemented as non-trivial custom data structures | Section B modules all present and tested |
| SC-2 | Routing correctness | 100% of emitted two-qubit gates lie on a physical coupling edge |
| SC-3 | SWAP minimization vs naive baseline | insertedswapcount(QRoute) ::max();

using LogicalQubit  = uint32_t;
using PhysicalQubit = uint32_t;
using NodeIndex     = uint32_t;
using ClbitIndex    = uint32_t;

enum class OpCode : uint8_t {
    Nop, OpenQasmHeader, Include, QReg, CReg,
    H, X, Y, Z, RX, RY, RZ,
    CX, SWAP, Barrier, Measure, Reset
};

const char* opcodetostring(OpCode op) noexcept;
bool issinglequbit_gate(OpCode op) noexcept;
bool istwoqubit_gate(OpCode op) noexcept;
bool isclassicalinstruction(OpCode op) noexcept;  // measure/barrier/reset

struct ParseError {
    std::string message;
    uint32_t    line;
    uint32_t    column;
};

} // namespace qroute

common/constants.hpp

namespace qroute {
inline constexpr double kEpsilon          = 1e-12;
inline constexpr double kInfinity         = std::numeric_limits::infinity();
inline constexpr double kDefaultT1Us      = 50.0;
inline constexpr double kDefaultT2Us      = 70.0;
inline constexpr double kDefaultFidelityThreshold = 0.90;
inline constexpr double kDefaultAlertSlackUs      = 5.0;
inline constexpr double kSingleQubitNs    = 35.0;
inline constexpr double kTwoQubitNs       = 300.0;
inline constexpr double kMeasureNs        = 800.0;
inline constexpr uint32_t kMaxMcmDimShift = 20;   // cap 2^20 in derived chains
}

common/safe_arithmetic.hpp — [INV-4 MANDATORY]

struct SafeArithmetic {
    static constexpr uint64t kMaxU64 = std::numericlimits::max();

    // Returns true on overflow; result saturated to kMaxU64.
    static bool multiplyu64(uint64t a, uint64t b, uint64t& out) noexcept;
    static bool addu64(uint64t a, uint64t b, uint64t& out) noexcept;

    // out = base + (abc), saturated. Returns true on any overflow.
    static bool saturatedtripleproduct_add(
        uint64t a, uint64t b, uint64t c, uint64t base, uint64_t& out) noexcept;

    // FNV-1a 64-bit over a byte buffer; used for determinism_hash.
    static uint64t fnv1a64(const void* data, sizet len, uint64_t seed = 1469598103934665603ULL) noexcept;
    static uint64t fnv1a64str(const std::string& s, uint64_t seed = 1469598103934665603ULL) noexcept;
};

Implementation requirements:

inline bool SafeArithmetic::multiplyu64(uint64t a, uint64t b, uint64t& out) noexcept {
    if (a == 0 || b == 0) { out = 0; return false; }
    if (a > kMaxU64 / b)  { out = kMaxU64; return true; }
    out = a * b; return false;
}
inline bool SafeArithmetic::addu64(uint64t a, uint64t b, uint64t& out) noexcept {
    if (a > kMaxU64 - b) { out = kMaxU64; return true; }
    out = a + b; return false;
}

saturatedtripleproduct_add must chain: ab → (ab)*c → +base, setting overflow on the first failure and returning kMaxU64.

common/file_io.hpp

namespace qroute {
std::string readtextfile(const std::string& path);           // throws std::runtime_error
std::vector read_lines(const std::string& path);  // strips \r, keeps line numbers 1-based
bool writetextfile(const std::string& path, const std::string& content);
std::vector split_tokens(const std::string& s);   // whitespace split
std::string strip_comment(const std::string& line);            // removes '#' and '//' comments
}

common/deterministic_time.hpp

namespace qroute {
class ScopedTimerUs {
public:
    explicit ScopedTimerUs(double& sink_us);
    ~ScopedTimerUs();
    double elapsed_us() const noexcept;
};
double now_us() noexcept;
}

B.1 UNIT 1 — Trees: Ingestion Layer (unit1_ast/)

B.1.1 Purpose

Ingest OpenQASM 2.0 traces into a cache-efficient AST supporting O(1)-auxiliary-space iterative in-order traversal via threaded pointer mechanics, with an AVL symbol table for balanced identifier resolution.

Syllabus coverage: Binary Trees, BST, AVL Trees, Threaded Binary Trees, Binary Heaps (baseline compared in Unit 5), traversal optimization, priority-queue justification.

B.1.2 Lexer (unit1_ast/lexer.hpp)

namespace qroute::unit1 {

enum class TokenType : uint8_t {
    EndOfFile, OpenQasm, Include, QReg, CReg, Barrier, Measure,
    Ident, Integer, Float, Pi, String,
    GateH, GateX, GateY, GateZ, GateRX, GateRY, GateRZ, GateCX, GateSwap,
    LParen, RParen, LBracket, RBracket, Comma, Semicolon, Arrow,
    Plus, Minus, Star, Slash
};

struct Token {
    TokenType   type = TokenType::EndOfFile;
    std::string text;
    double      numeric = 0.0;
    uint32_t    line = 0;
    uint32_t    column = 0;
};

class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector tokenize();       // throws std::invalid_argument on malformed token with line/col
    static std::string tokentypename(TokenType t) noexcept;
};

} // namespace qroute::unit1

Lexer requirements:

| Req | Detail |
|---|---|
| L-1 | Track 1-based line and column for every token. |
| L-2 | Strip // line comments and # comments; ignore blank lines. |
| L-3 | Accept integers, decimals (0.45, .5, 1.), scientific notation (1e-3). |
| L-4 | Recognize pi as TokenType::Pi with numeric = 3.14159265358979323846. |
| L-5 | Recognize -> as Arrow; never mis-lex as Minus. |
| L-6 | Recognize quoted strings for include "qelib1.inc";. |
| L-7 | Unknown character → std::invalid_argument with line:column. |

B.1.3 Parser + Grammar Subset (unit1ast/qasmparser.hpp)

Supported statement set (strict):

OPENQASM 2.0;
include "qelib1.inc";
qreg [];
creg [];
h  ;            x ;   y ;   z ;
rx  ;     ry  ;   rz  ;
cx , ;
swap , ;
barrier  (, )* ;
measure  -> ;

where:

qarg  := IDENT '[' INT ']'
carg  := IDENT '[' INT ']'
expr  := term (('+' | '-') term)*
term  := factor (('' | '/') factor)
factor:= ('+'|'-') factor | NUMBER | 'pi' | '(' expr ')'

Expression evaluation: compile-time constant folding into double. No runtime symbolic expressions.

namespace qroute::unit1 {

struct ParsedProgram {
    ThreadedAST                    ast;
    AvlSymbolTable   registers;
    uint32t                       totallogical_qubits = 0;
    uint32t                       totalclassical_bits  = 0;
    std::vector        warnings;
};

struct RegisterInfo {
    std::string name;
    bool        is_quantum = false;
    uint32_t    width      = 0;
    uint32t    baseindex = 0;   // flattened global qubit/clbit offset
};

class QasmParser {
public:
    ParsedProgram parse(const std::string& source);
    static ParsedProgram parse_file(const std::string& path);
};

} // namespace qroute::unit1

Parser requirements:

| Req | Detail |
|---|---|
| P-1 | OPENQASM 2.0; must be the first statement; otherwise std::invalid_argument. |
| P-2 | include is accepted and recorded as a node but performs no file I/O. |
| P-3 | All qreg/creg declarations must precede any gate that references them. |
| P-4 | Duplicate register name → error. |
| P-5 | Qubit index out of declared range → error with line. |
| P-6 | cx q[i], q[i]; (identical operands) → error. |
| P-7 | Arity mismatch (e.g., rx without parameter, h with two operands) → error. |
| P-8 | Unsupported gate name → error listing the supported set. |
| P-9 | Flattened logical qubit id = register.baseindex + localindex, in declaration order. |
| P-10 | Exactly one ASTNode appended per source statement (including declarations and barriers). |
| P-11 | Parser must call ThreadedAST::reserve(statementcountestimate) before appending. |
| P-12 | After appending, call buildfromsequence() then finalize_threads() exactly once. |

B.1.4 Hot/Cold Memory Split — Size Contracts (MANDATORY)

unit1ast/astnode.hpp

namespace qroute::unit1 {

struct ASTNode {                      // HOT — traversal critical
    uint32_t left          = kNil;    // child index OR thread index
    uint32_t right         = kNil;    // child index OR thread index
    uint32_t payload       = kNil;    // index into cold payload array
    uint32_t seq           = 0;       // source-order sequence number
    uint32t sourceline   = 0;
    OpCode   op            = OpCode::Nop;
    uint8t  leftthread   = 0;       // 1 => left is inorder predecessor
    uint8t  rightthread  = 0;       // 1 => right is inorder successor
    uint8_t  reserved      = 0;
};

static_assert(sizeof(ASTNode) == 24, "ASTNode must be exactly 24 bytes (cache-line packing contract)");
static_assert(alignof(ASTNode) == 4, "ASTNode must not be over-aligned");

} // namespace qroute::unit1

unit1ast/gatepayload.hpp

namespace qroute::unit1 {

struct GatePayload {                                  // COLD — semantic payload
    std::array qubits{};                 // 16
    std::array params{};                 // 32
    std::array clbits{};                 //  8
    uint32t sourceline = 0;                         //  4
    uint8t  qubitcount = 0;                         //  1
    uint8t  paramcount = 0;                         //  1
    uint8t  clbitcount = 0;                         //  1
    uint8_t  flags       = 0;                         //  1
};

static_assert(sizeof(GatePayload) == 64, "GatePayload must be exactly 64 bytes (one cache line)");

} // namespace qroute::unit1

Cache rationale (must appear in code comments):
ASTNode (24 B) packs ~2.6 nodes per 64-byte cache line; traversal touches only left, right, leftthread, rightthread. The 64-byte GatePayload is dereferenced only when a visitor executes the instruction, preventing cache pollution during scans.

B.1.5 ThreadedAST (unit1ast/threadedast.hpp)

namespace qroute::unit1 {

class ThreadedAST {
public:
    ThreadedAST() = default;

    void reserve(sizet nodecount);

    // Appends a node in source order. Returns its index.
    NodeIndex append(OpCode op, GatePayload payload, uint32t sourceline);

    // Builds a balanced binary projection over [begin, end). Inorder == source order.
    NodeIndex build_balanced(NodeIndex begin, NodeIndex end);
    void      buildfromsequence();          // build_balanced(0, size())

    // Converts every absent child into an inorder predecessor/successor thread.
    void finalize_threads();

    NodeIndex root() const noexcept;
    size_t    size() const noexcept;

    const ASTNode&     node(NodeIndex i) const;
    const GatePayload& payload(NodeIndex i) const;

    // ---- O(1)-auxiliary iterative traversal primitives ----
    NodeIndex leftmost(NodeIndex start) const noexcept;
    NodeIndex next(NodeIndex i) const noexcept;
    NodeIndex prev(NodeIndex i) const noexcept;   // symmetric, optional but recommended

    template 
    void traverse(Fn fn) const {
        for (NodeIndex cur = leftmost(root_); cur != kNil; cur = next(cur)) {
            fn(cur, node(cur), payload(node(cur).payload));
        }
    }

    // Test/validation helpers (may use auxiliary memory; NOT used in hot path)
    std::vector inorder_reference() const;   // stack-based oracle
    bool validate_threads() const noexcept;
    uint32_t height() const noexcept;
    bool is_balanced() const noexcept;                  // |hL - hR|      nodes_;
    std::vector payloads_;
    NodeIndex                root_ = kNil;
};

} // namespace qroute::unit1

Threading invariants (must be asserted by validate_threads()):

| ID | Invariant |
|---|---|
| TH-1 | left_thread == 0 ⇒ left is a left child index or kNil-free subtree root |
| TH-2 | left_thread == 1 ⇒ left is the inorder predecessor, or kNil for the first node |
| TH-3 | right_thread == 0 ⇒ right is a right child index |
| TH-4 | right_thread == 1 ⇒ right is the inorder successor, or kNil for the last node |
| TH-5 | Inorder traversal via leftmost+next visits every node exactly once |
| TH-6 | Traversal order equals ascending seq |
| TH-7 | Tree height satisfies height 
class AvlSymbolTable {
public:
    struct Entry { std::string key; Value value; };

    bool        insert(const std::string& key, Value value);   // false if key exists
    bool        contains(const std::string& key) const noexcept;
    const Value* find(const std::string& key) const noexcept;
    size_t      size() const noexcept;
    uint32_t    height() const noexcept;
    bool        is_balanced() const noexcept;                  // |bf|  inorder() const;                        // ascending key order

private:
    struct Node {
        std::string key;
        Value       value{};
        Node*       left  = nullptr;   // non-owning structural link
        Node*       right = nullptr;
        int32_t     height = 1;
    };
    std::vector> pool;   // OWNS all nodes (RAII)
    Node* root_ = nullptr;

    static int32_t h(const Node* n) noexcept;
    static int32_t bf(const Node* n) noexcept;
    Node rotate_left(Node n) noexcept;
    Node rotate_right(Node n) noexcept;
    Node rebalance(Node n) noexcept;          // handles LL, RR, LR, RL
};

} // namespace qroute::unit1

AVL requirements:

| Req | Detail |
|---|---|
| A-1 | All four rotation cases implemented: LL, RR, LR, RL. |
| A-2 | Height stored per node; bf ∈ {-1, 0, +1} invariant after every insert. |
| A-3 | Node ownership via std::vector> pool; zero raw new/delete in callers. |
| A-4 | is_balanced() performs an explicit (non-recursive-depth-unsafe) iterative check for tests. |
| A-5 | For 10,000 sequential (worst-case) inserts: height() = 0
    double   gatetimens   = 0.0;
};

static_assert(sizeof(PhysicalEdge) == 32, "PhysicalEdge must be exactly 32 bytes");

} // namespace qroute::unit2

B.2.3 PhysicalCouplingGraph (unit2graph/physicalcoupling_graph.hpp)

namespace qroute::unit2 {

class PhysicalCouplingGraph {
public:
    explicit PhysicalCouplingGraph(uint32t vertexcount);

    // Topology construction (validates fidelity domain, rejects self-loops/duplicates)
    void addundirectededge(uint32t u, uint32t v, double fidelity, double gatetimens);

    static PhysicalCouplingGraph loadfromfile(const std::string& path);   // coupling.map schema (D.3)
    void sortadjacency();      // sort each adj[u] by to ascending -- MANDATORY for determinism

    uint32t vertexcount() const noexcept;
    sizet   edgecount()   const noexcept;
    const std::vector& neighbors(uint32_t u) const;
    bool     hasedge(uint32t u, uint32_t v) const noexcept;
    double   edgefidelity(uint32t u, uint32_t v) const;
    double   edgeweight(uint32t u, uint32_t v) const;

    // ---- Traversals / connectivity ----
    bool is_connected() const noexcept;
    std::vector bfsorder(uint32_t source) const;
    std::vector dfsorder(uint32_t source) const;      // iterative, explicit stack
    std::vector connectedcomponent(uint32_t source) const;
    uint32t connectedcomponent_count() const;

    // ---- Shortest paths: [INV-1] std::priority_queue ONLY ----
    std::vector dijkstra(uint32t source, std::vector* prev = nullptr) const;
    std::vector shortestpath(uint32t source, uint32t target) const;   // empty if unreachable

    // ---- All-pairs cache used by Unit 4 lower bounds ----
    void computeallpairs();                                   // BFS hops + Dijkstra weights
    uint16t hops(uint32t u, uint32t v) const;                // UINT16MAX if unreachable
    double   weighteddistance(uint32t u, uint32_t v) const;

    // ---- Spanning trees (syllabus completeness) ----
    std::vector> mst_prim() const;
    std::vector> mst_kruskal() const;
    double msttotalweight(const std::vector>& mst) const;

    // ---- Validation oracles (TEST/BENCH ONLY, never used in routing hot path) ----
    std::vector bellmanford(uint32t source) const;
    std::vector> floyd_warshall() const;
    bool validateagainstoracles(double tol = 1e-9) const;

private:
    uint32t n = 0;
    sizet   edgecount_ = 0;
    std::vector> adj_;

    // Row-major dense caches: index = u * n_ + v
    std::vector hopmatrix_;
    std::vector   costmatrix;
    bool                  apspcomputed = false;
};

} // namespace qroute::unit2

B.2.4 Dijkstra Specification — [INV-1]

Mandatory implementation shape:

std::vector PhysicalCouplingGraph::dijkstra(
    uint32t source, std::vector* prev) const
{
    std::vector   dist(n_, kInfinity);
    if (prev) prev->assign(n_, kNil);
    dist[source] = 0.0;

    // [INV-1] binary heap ONLY. Deterministic tie-break: (cost ASC, vertex ASC).
    std::priority_queue,
        std::vector>,
        std::greater>
    > pq;

    pq.push({0.0, source});

    while (!pq.empty()) {
        const auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u] + kEpsilon) continue;            // stale entry (lazy deletion)

        for (const PhysicalEdge& e : adj_[u]) {
            const double nd = d + e.weight;
            if (nd + kEpsilon  + std::greater<>. |
| D-3 | Lazy deletion (stale-entry skip) instead of explicit decrease-key. |
| D-4 | All weights verified >= 0 at edge-insertion time. |
| D-5 | adj sorted ascending by to after load (sortadjacency()), guaranteeing reproducible relaxation order. |
| D-6 | shortest_path reconstructs via prev and must assert consecutive adjacency of every returned pair. |
| D-7 | Unreachable target ⇒ return empty vector; callers must raise RoutingError. |
| D-8 | Complexity target: O((V + E) log V) time, O(V + E) space. |

B.2.5 Placement Map (unit2graph/placementmap.hpp)

namespace qroute::unit2 {

class PlacementMap {
public:
    PlacementMap(uint32t logicalcount, uint32t physicalcount);

    void set(LogicalQubit l, PhysicalQubit p);
    void load(const std::vector& logicalto_physical);   // -1 == unmapped
    void applyfull(std::vector logicaltophysical);

    PhysicalQubit physical_of(LogicalQubit l) const;
    LogicalQubit  logical_of(PhysicalQubit p) const;              // kNil if idle
    bool          is_adjacent(const PhysicalCouplingGraph& g, LogicalQubit a, LogicalQubit b) const;

    // Swaps the two PHYSICAL occupants; updates both directions. Core routing primitive.
    void swap_physical(PhysicalQubit a, PhysicalQubit b);

    const std::vector& logicalto_physical() const noexcept;
    const std::vector& physicalto_logical() const noexcept;

    bool validate_bijection() const noexcept;   // no two logical qubits share a physical qubit

private:
    std::vector logicaltophysical;
    std::vector physicaltological;
};

} // namespace qroute::unit2

B.2.6 SWAP Synthesizer (unit2graph/swapsynthesizer.hpp)

namespace qroute::unit2 {

struct EmittedGate {
    OpCode        op = OpCode::CX;
    PhysicalQubit control = kNil;
    PhysicalQubit target  = kNil;
    double        fidelity   = 1.0;
    double        weight     = 0.0;
    double        duration_ns = 0.0;
    uint32t      sourceline = 0;
    bool          issynthethicswap_component = false;
};

struct RoutingPlan {
    std::vector gates;        // fully decomposed: every SWAP -> 3 CX
    std::vector> swap_edges;  // logical SWAP edges (for counting)
    uint32t swapcount      = 0;
    uint32t addedcnotcount = 0;         // == 3 * swapcount
    double   path_weight      = 0.0;
    double   path_fidelity    = 1.0;
    uint16t hopdistance     = 0;
};

class SwapSynthesizer {
public:
    // Routes logical gate (a,b). Emits SWAP decomposition + final CX. MUTATES map.
    RoutingPlan route(const PhysicalCouplingGraph& g,
                      PlacementMap& map,
                      LogicalQubit a,
                      LogicalQubit b) const;

    // Pure helper: given path p0..pd, return d-1 swap edges moving p0 toward pd.
    static std::vector>
    swapedgesforpath(const std::vector& path);

    // Emits CX(x,y), CX(y,x), CX(x,y) for one logical SWAP.
    static std::vector decompose_swap(const PhysicalCouplingGraph& g,
                                                   PhysicalQubit x, PhysicalQubit y);
};

} // namespace qroute::unit2

SWAP synthesis algorithm (mandatory):

route(g, map, a, b):
  pa = map.physicalof(a); pb = map.physicalof(b)
  if g.hasedge(pa, pb):  emit CX(pa,pb); return plan(swapcount=0)

  path = g.shortest_path(pa, pb)                 # p0=pa ... pd=pb, d = len-1
  if path.empty(): throw RoutingError("logical pair unreachable on coupling graph")

  # Move the qubit at pa toward pb along the path (d-1 SWAPs)
  for r in 0 .. d-2:
      x = path[r]; y = path[r+1]
      emit decompose_swap(g, x, y)               # exactly 3 CX gates
      plan.swapedges.pushback({x, y})
      map.swap_physical(x, y)                    # MUST keep bijection valid

  # Invariant: a now resides at path[d-1], adjacent to b at path[d]
  assert g.hasedge(map.physicalof(a), map.physical_of(b))
  emit CX(map.physicalof(a), map.physicalof(b))
  plan.swap_count = d-1
  plan.addedcnotcount = 3 * (d-1)

Synthesizer requirements:

| Req | Detail |
|---|---|
| S-1 | Every emitted two-qubit gate MUST satisfy g.has_edge(control, target). Hard assertion. |
| S-2 | Exactly 3 CX per SWAP; addedcnotcount == 3 * swap_count invariant. |
| S-3 | map.validatebijection() must hold after every swapphysical. |
| S-4 | SWAP edge fidelity: Fswap = Fcx^3, weight wswap = 3 * wedge. |
| S-5 | Path weight accumulation in log space; pathfidelity = exp(-pathweight). |
| S-6 | Unreachable endpoints ⇒ std::runtime_error (never emit an illegal gate). |
| S-7 | Determinism: identical inputs produce identical gates vectors. |

B.2.7 Unit 2 Acceptance Criteria

unit2_acceptance:
  - "static_assert(sizeof(PhysicalEdge) == 32) compiles."
  - "Invalid fidelity (1), self-loop, duplicate edge all throw with descriptive messages."
  - "dijkstra() distances match floydwarshall() on linear5, grid3x3, heavyhex7 (T2.4)."
  - "bellman_ford() agrees with dijkstra() on the same topologies (T2.5)."
  - "shortest_path() returns a valid adjacent-vertex chain with correct endpoints (T2.6)."
  - "mstprim() and mstkruskal() produce identical total weight (T2.8)."
  - "SwapSynthesizer emits ONLY adjacent two-qubit gates for every test circuit (T2.10)."
  - "PlacementMap bijection invariant holds after every swap across the full trace (T2.11)."
  - "addedcnotcount == 3 * swap_count in all cases (T2.12)."
  - "grep gate: zero occurrences of 'FibonacciHeap' in unit2 sources/headers [INV-1]."

B.2.8 Unit 2 Test Matrix

| Test ID | Description |
|---|---|
| T2.1 | Edge insertion validation (fidelity domain, self-loop, duplicate, OOB vertex) |
| T2.2 | weight == -log(fidelity) and weight >= 0 for randomized fidelities |
| T2.3 | isconnected() / connectedcomponent_count() on connected and split topologies |
| T2.4 | Dijkstra ≡ Floyd-Warshall on 3 topologies |
| T2.5 | Dijkstra ≡ Bellman-Ford on 3 topologies |
| T2.6 | Path validity: consecutive adjacency + endpoint correctness + unreachable ⇒ empty |
| T2.7 | BFS hop matrix ≡ analytic |i-j| on linear topology |
| T2.8 | Prim total weight == Kruskal total weight |
| T2.9 | Determinism: 20 identical Dijkstra runs produce byte-identical prev arrays |
| T2.10 | SwapSynthesizer legality across a 20-gate trace on grid_3x3 |
| T2.11 | PlacementMap bijection invariant maintained across full trace |
| T2.12 | SWAP accounting identity addedcnot == 3 * swapcount |
| T2.13 | INV-1 static gate: no FibonacciHeap symbol in unit2 translation units |

B.3 UNIT 3 — Dynamic Programming: Contraction Planning (unit3_dp/)

B.3.1 Purpose and Scope Limit [INV-3]

Compute the globally optimal parenthesization of a unitary/tensor contraction chain to minimize scalar floating-point operations during classical verification.

STRICTLY IN SCOPE:
Cost table m[i,j]
Split table s[i,j]
Parenthesization string
Contraction merge order
Left-to-right greedy baseline cost
Absolute + percentage savings
Overflow detection/saturation flag

STRICTLY OUT OF SCOPE (forbidden):
Quantum state vectors / complex amplitudes / std::complex
Any 2^n memory allocation
BLAS, LAPACK, Eigen, Armadillo, xtensor, cuBLAS
Actual matrix multiplication or numerical unitary application

B.3.2 Output Structures

namespace qroute::unit3 {

struct ContractionPlan {
    uint32t              chainlength = 0;         // k matrices
    std::vector dimensions;               // p[0..k]

    uint64t              optimalcost = 0;         // m[0][k-1]
    uint64t              lefttorightcost = 0;
    uint64_t              savings = 0;              // LR - optimal (0 if LR > merge_order;  // (i,j) pairs in execution order

    std::vector costtable;               // flattened k x k row-major  (m)
    std::vector splittable;              // flattened k x k row-major  (s)

    bool                  overflow_detected = false;
};

} // namespace qroute::unit3

B.3.3 MatrixChainOptimizer

namespace qroute::unit3 {

class MatrixChainOptimizer {
public:
    // dimensions.size() must be >= 2; dimensions[i] > 0 for all i.
    explicit MatrixChainOptimizer(std::vector dimensions);

    ContractionPlan build_plan();

    uint64t optimalcost() const noexcept;
    const std::vector& costtable() const noexcept;   // flattened row-major m
    const std::vector& splittable() const noexcept;  // flattened row-major s
    bool overflow_detected() const noexcept;

    // Deterministic chain derivation from a parsed circuit (see B.3.6)
    static std::vector derivechainfromast(const unit1::ThreadedAST& ast);

    static std::vector loadchain_file(const std::string& path);  // contraction.chain schema (D.4)

private:
    std::vector p;
    uint32t              n = 0;              // matrix count = p_.size() - 1
    std::vector m;                  // n * n, row-major
    std::vector s;                  // n * n, row-major
    bool                  overflow_ = false;

    uint64t& m(uint32t i, uint32_t j) noexcept;
    uint32t& s(uint32t i, uint32_t j) noexcept;
    uint64t  mat(uint32t i, uint32t j) const noexcept;
    uint32t  sat(uint32t i, uint32t j) const noexcept;

    void run_dp();
    uint64t computelefttoright_cost();
    void buildparenthesization(uint32t i, uint32_t j, std::string& out) const;
    void buildmergeorder(uint32t i, uint32t j,
                           std::vector>& out) const;
};

} // namespace qroute::unit3

B.3.4 MCM Recurrence (Mandatory Implementation)

Zero-based tables. For \(k\) matrices with dimensions \(p[0..k]\):

\[
m[i][i] = 0
\]

\[
m[i][j] = \min_{i \le r (p.size() - 1);
    m.assign(staticcast(n) * n_, 0ULL);
    s.assign(staticcast(n) * n_, 0U);

    for (uint32t len = 2; len  optimal) ? (lr - optimal) : 0ULL;
plan.savings_percent  = (lr > 0) ? 100.0 * double(plan.savings) / double(lr) : 0.0;

B.3.6 Deterministic Chain Derivation from the AST

When no --chain file is supplied, derive the contraction chain from the parsed circuit:

derivechainfrom_ast(ast):
  layers = greedylayerpartition(ast)        # deterministic, see below
  dims   = []
  for each layer L in layers (ascending order):
      q = number of distinct logical qubits touched by L
      q = clamp(q, 1, kMaxMcmDimShift)
      dims.push_back(1ULL  optimal_cost == 26000 (T3.2)."
  - "Chain {30,35,15,5,10,20,25} -> optimal_cost == 15125 (T3.3)."
  - "DP result equals brute-force optimum for all k >& matrix() const noexcept;

    std::vector degree() const;

    // Deterministic ordering: degree DESC, then id ASC.
    std::vector degreesorted_order() const;

    std::vector> edges() const;   // sorted ascending by (i,j)
};

} // namespace qroute::unit4

Requirements:

| Req | Detail |
|---|---|
| LG-1 | \(\alpha_{ij}\) = count of two-qubit gates between logical \(i\) and \(j\) within the lookahead window. |
| LG-2 | Symmetric matrix; diagonal always 0. |
| LG-3 | edges() returned in ascending (i,j) order for deterministic bound evaluation. |
| LG-4 | degreesortedorder() ties broken by ascending logical id. |

B.4.3 Placement Result and Configuration

namespace qroute::unit4 {

struct PlacementResult {
    double                 best_cost = kInfinity;
    std::vector   logicalto_physical;   // -1 == unmapped
    uint64t               nodesexpanded  = 0;
    uint64t               branchespruned = 0;
    uint32t               maxstack_depth = 0;
    bool                   completed       = false;   // false if aborted by time limit
    bool                   feasible        = false;
    double                 searchtimeus  = 0.0;
    double                 greedyupperbound_cost = kInfinity;
};

struct PlacementConfig {
    uint32t logicalcount = 0;
    uint32t physicalcount = 0;
    const unit2::PhysicalCouplingGraph* graph = nullptr;   // non-owning, must outlive solver
    LookaheadGraph lookahead;
    double   timelimitms = 1000.0;      // & bestassignment() const noexcept;
    uint32t maxstackdepth() const noexcept;      // MUST be  variableorder_;                 // degree-sorted branching order
    std::vector  currentl2p_;                    // size n, -1 == unassigned
    std::vector  currentused_;                   // size m, 1 == occupied
    std::vector    stack_;                          // explicit DFS stack, reserved n+1

    double                bestcost = kInfinity;          // global upper bound register B
    std::vector  bestassignment_;

    uint64t              nodesexpanded_  = 0;
    uint64t              branchespruned_ = 0;
    uint32t              maxstackdepth = 0;

    // Precomputed lower-bound acceleration structures
    std::vector>> sorteddistfrom_;  // per physical: (dist, phys) ASC
    uint16t              minpairdistance = 1;

    void   precomputeboundstructures();
    double greedyupperbound();
    double assignincrementalcost(uint32t logicalqubit, uint32t physicalqubit) const;
    double lowerbound(double assignedcost) const;
    uint16t mindistancetounused(uint32t physicalqubit) const;
    void   assign(uint32t logicalqubit, uint32t physicalqubit);
    void   undoassign(uint32t logicalqubit, uint32t physical_qubit);
    bool   budget_exhausted() const noexcept;
};

} // namespace qroute::unit4

B.4.5 Admissible Lower Bound

Let \(A\) = assigned logical qubits, \(U\) = unused physical qubits. For each lookahead edge \((i,j)\):

\[
\delta_{ij} =
\begin{cases}
d\big(\pi(i), \pi(j)\big), & i,j \in A \\[4pt]
\min\limits_{p \in U} d\big(\pi(i), p\big), & i \in A,\; j \notin A \\[4pt]
\min\limits_{p \in U} d\big(\pi(j), p\big), & j \in A,\; i \notin A \\[4pt]
\text{min\pair\distance}, & i,j \notin A
\end{cases}
\]

\[
LB(\text{node}) = \text{assigned\cost} + \sum{(i,j)\in EL,\; i\notin A \text{ or } j \notin A} \alpha{ij}\,\delta_{ij}
\]

Admissibility requirement: \(LB \le\) true cost of every completion of the node. Test T4.5 verifies this by comparing against exhaustive optimum on small instances.

Acceleration requirement: mindistancetounused(p) must scan sorteddistfrom[p] (precomputed ascending distance list) and return the first entry whose physical qubit is currently unused. Expected cost \(O(1)\)–\(O(m)\), never \(O(m^2)\).

B.4.6 Pruning Rule

With global upper bound register \(B\):

\[
LB(\text{node}) \ge B - \varepsilon \;\Rightarrow\; \text{prune},\quad \texttt{branches\pruned\++}
\]

\(B\) is initialized by greedyupperbound() and tightened whenever a complete assignment improves it.

B.4.7 DFS Search Loop (Mandatory Shape)

PlacementResult QubitPlacementBAndB::solve() {
    precomputeboundstructures();
    variableorder = cfg.lookahead.degreesorted_order();

    bestcost = greedyupperbound();          // B initialized from a feasible solution
    bestassignment = / greedy assignment /;

    const uint32t n = cfg.logical_count;
    currentl2p.assign(n, -1);
    currentused.assign(cfg.physicalcount, 0);

    stack_.clear();
    stack_.reserve(n + 1);                      // SINGLE reservation: no growth during search
    stack.pushback(Frame{0, 0, kNil, kNil, 0.0});

    while (!stack_.empty()) {
        Frame& f = stack_.back();
        maxstackdepth = std::max(maxstackdepth, (uint32t)stack_.size());

        if (budget_exhausted()) break;

        if (f.depth == n) {                                  // complete assignment
            if (f.assignedcost  0 on data/circuit.qasm + linear5 topology (T4.6)."
  - "nodes_expanded  m) returns feasible == false without crashing (T4.11)."
  - "Disconnected coupling graph handled: unreachable pairs treated as cost kInfinity, reported infeasible if unavoidable (T4.12)."
  - "grep gate: zero occurrences of std::priority_queue / FibonacciHeap / std::multiset in unit4 [INV-2]."
  - "Deterministic: 20 consecutive solves produce identical logicaltophysical vectors (T4.13)."

B.4.10 Unit 4 Test Matrix

| Test ID | Description |
|---|---|
| T4.1 | LookaheadGraph build: α_ij counts, symmetry, degree ordering determinism |
| T4.2 | Greedy upper bound produces a feasible injective assignment |
| T4.3 | B&B ≡ brute force on (4,4), (5,4), (6,5), (6,6) |
| T4.4 | Cost function matches manual computation on a hand-verified instance |
| T4.5 | Lower-bound admissibility verified against exhaustive completion costs |
| T4.6 | Pruning activity: branchespruned > 0 and nodesexpanded recorded |
| T4.7 | maxstackdepth()  m, disconnected required pairs |
| T4.12 | Weighted-distance mode (usehopdistance == false) correctness |
| T4.13 | Determinism across repeated solves |
| T4.14 | INV-2 static gate: no best-first containers in unit4 |

B.5 UNIT 5 — Advanced Data Structures: Decoherence Telemetry (unit5_heap/)

B.5.1 Purpose

Broker real-time decoherence threat alerts using a custom Fibonacci heap exploiting \(O(1)\) amortized insert and decrease_key. The heap is used exclusively here — never in Unit 2 routing [INV-1].

Syllabus coverage: Red-Black Trees, B/B+ Trees, Binomial Heaps, Fibonacci Heaps (comparative justification documented in docs/); Fibonacci heap implemented natively.

B.5.2 FibonacciHeap (unit5heap/fibonacciheap.hpp)

namespace qroute::unit5 {

template 
class FibonacciHeap {
public:
    struct Node {
        Key    key{};
        Value  value{};
        Node*  parent = nullptr;
        Node*  child  = nullptr;
        Node*  left   = nullptr;    // circular doubly-linked sibling list
        Node*  right  = nullptr;
        int32_t degree = 0;
        bool    mark   = false;     // cascading-cut marker
        bool    in_heap = false;
    };

    using Handle = Node*;

    FibonacciHeap() = default;
    ~FibonacciHeap() = default;                       // pool_ owns all nodes (RAII)

    FibonacciHeap(const FibonacciHeap&)            = delete;
    FibonacciHeap& operator=(const FibonacciHeap&) = delete;
    FibonacciHeap(FibonacciHeap&&)                 = delete;   // handles would dangle
    FibonacciHeap& operator=(FibonacciHeap&&)      = delete;

    // ---- Core operations ----
    Handle insert(const Key& key, const Value& value);            // O(1) amortized
    bool   empty() const noexcept;
    size_t size() const noexcept;
    const Node& minimum() const;                                   // throws if empty
    std::pair extract_min();                           // O(log n) amortized
    void decreasekey(Handle h, const Key& newkey);               // O(1) amortized
    bool contains_handle(Handle h) const noexcept;

    // Copy-in merge (avoids cross-pool pointer aliasing). Other heap is emptied.
    void absorb(FibonacciHeap& other);

    // ---- Introspection for tests/benchmarks ----
    sizet   poolsize() const noexcept;
    sizet   freelist_size() const noexcept;
    sizet   rootlist_size() const;
    uint32t maxdegree() const noexcept;
    bool     validate_structure() const;      // degree bounds, circular list integrity, heap order

    struct Stats {
        uint64t inserts = 0, decreasekeys = 0, extract_mins = 0;
        uint64t cuts = 0, cascadingcuts = 0, links = 0, consolidations = 0;
        uint64t nodesallocated = 0, nodes_recycled = 0;
    };
    Stats stats() const noexcept;

private:
    std::deque     pool_;        // OWNS nodes; references/pointers remain stable
    std::vector   freelist;   // recycled nodes (handle invalidation documented)
    std::vector   degreetable; // scratch for consolidate(), reused across calls
    Node*  minroot = nullptr;
    sizet n        = 0;
    Stats  stats_{};

    Node* allocate(const Key& key, const Value& value);
    void  recycle(Node* n);
    void  addtoroot_list(Node* n);
    void  removefromlist(Node* n);
    void  consolidate();
    void  link(Node child, Node parent);
    void  cut(Node x, Node y);
    void  cascading_cut(Node* y);
};

} // namespace qroute::unit5

B.5.3 Fibonacci Heap Behavioral Requirements

| Req | Detail |
|---|---|
| FH-1 | Root list and child lists are circular doubly linked. n->left->right == n and n->right->left == n always. |
| FH-2 | insert is lazy: allocate, reset fields, splice into root list, update minroot, n_++. No consolidation. |
| FH-3 | extractmin splices children of minroot into the root list, recycles minroot_, then consolidate(). |
| FH-4 | consolidate() uses the reusable degreetable sized floor(log_phi(n)) + 2; must clear()/assign() without reallocating when capacity suffices. |
| FH-5 | link(child, parent) removes child from root list, makes it a child of parent, increments parent->degree, sets child->mark = false. |
| FH-6 | decreasekey cuts the node if it violates heap order with its parent, then performs cascadingcut using the mark bit. |
| FH-7 | decreasekey with newkey > h->key is a no-op returning false-equivalent behavior (must not corrupt structure). Add explicit guard. |
| FH-8 | Handles to extracted/recycled nodes become invalid; contains_handle must return false for them. |
| FH-9 | maxdegree()  guarantees pointer stability. Zero raw new/delete. |
| FH-11 | freelist recycles extracted nodes; nodes_recycled counted in Stats. |
| FH-12 | Move/copy operations deleted (handle stability contract). |
| FH-13 | Determinism: for equal keys, extractmin returns the earliest-inserted node. Implement via a monotonically increasing uint64t insertion_seq embedded in the key wrapper used by TelemetryBroker (see B.5.5). |

B.5.4 Binary Heap Baseline (unit5heap/binaryheap_baseline.hpp)

Implemented only for empirical comparison (Unit 5 vs binary heap) and as a test oracle. Must never be referenced from unit2_graph/.

namespace qroute::unit5 {

// Indexed binary heap supporting decrease_key, used as benchmark baseline + test oracle.
template 
class BinaryHeapBaseline {
public:
    using Handle = uint32_t;   // stable slot id
    Handle insert(const Key& key, const Value& value);
    void   decreasekey(Handle h, const Key& newkey);
    std::pair extract_min();
    bool   empty() const noexcept;
    size_t size() const noexcept;
    typename FibonacciHeap::Stats stats() const noexcept;   // comparable counters
private:
    std::vector> heap_;
    std::vector              indexof_;
    std::vector              slotof_;
};

} // namespace qroute::unit5

B.5.5 TelemetryBroker (unit5heap/telemetrybroker.hpp)

namespace qroute::unit5 {

struct TelemetryConfig {
    double t1_us                 = kDefaultT1Us;
    double t2_us                 = kDefaultT2Us;
    double fidelity_threshold    = kDefaultFidelityThreshold;
    double alertslackus        = kDefaultAlertSlackUs;
    double singlequbitns       = kSingleQubitNs;
    double twoqubitns          = kTwoQubitNs;
    double measure_ns            = kMeasureNs;
    static TelemetryConfig loadfromfile(const std::string& path);   // telemetry.conf schema (D.5)
};

struct Alert {
    PhysicalQubit qubit       = kNil;
    double        slack_us    = 0.0;
    double        elapsed_us  = 0.0;
    double        hazard_rate = 0.0;
    uint64t      gateindex  = 0;
};

struct QubitHealth {
    double elapsed_ns = 0.0;
    double t1_us      = kDefaultT1Us;
    double t2_us      = kDefaultT2Us;
    double slack_us   = kInfinity;
    bool   active     = false;
    FibonacciHeap, PhysicalQubit>::Handle handle = nullptr;
};

class TelemetryBroker {
public:
    TelemetryBroker(uint32t physicalqubit_count, TelemetryConfig cfg);

    // Called by the execution engine after every emitted physical gate.
    void ongateexecuted(PhysicalQubit q, double durationns, uint64t gate_index);
    void advanceglobaltime(double duration_ns);

    double elapsed_ns(PhysicalQubit q) const noexcept;
    double globalelapsedns() const noexcept;
    double slack_us(PhysicalQubit q) const noexcept;

    // Recomputes slack and performs insert or decrease_key [O(1) amortized].
    void update_slack(PhysicalQubit q);

    bool hascriticalalert() const noexcept;
    Alert extractmostcritical();
    std::vector draincritical();          // all alerts with slack , PhysicalQubit>::Stats heapstats() const noexcept;

private:
    TelemetryConfig            cfg_;
    std::vector   health_;
    FibonacciHeap, PhysicalQubit> heap;   // key = (slackus, insertionseq)
    uint64t                   insertionseq_ = 0;
    double                     globalelapsedns_ = 0.0;
    uint64t                   decreasekeycount = 0;
    uint64t                   insertcount_ = 0;

    double hazard_rate(const QubitHealth& h) const noexcept;
    double criticaltimeus(const QubitHealth& h) const noexcept;
};

} // namespace qroute::unit5

B.5.6 Decoherence Slack Model (Mandatory Formulas)

Hazard rate for physical qubit \(q\):

\[
rq = \frac{1}{T1} + \frac{1}{T_2}
\]

Critical time for threshold fidelity \(F^*\):

\[
tq^{*} = \frac{-\ln(F^*)}{rq}
\]

Accumulated execution time (converted to microseconds):

\[
tq = \frac{\texttt{elapsed\ns}_q}{1000}
\]

Remaining slack (heap key):

\[
sq = tq^{*} - t_q
\]

Priority semantics: smaller \(s_q\) ⇒ more urgent ⇒ nearer heap minimum.

Alert condition:

\[
sq \le \texttt{alert\slack\_us}
\]

Key type: std::pair = (slackus, insertion_seq) to guarantee deterministic FIFO tie-breaking [FH-13].

B.5.7 Unit 5 Acceptance Criteria

unit5_acceptance:
  - "validate_structure() true after every operation in a 100k-op seeded random sequence (T5.2)."
  - "extract_min key sequence identical to BinaryHeapBaseline on the same seeded op stream (T5.3)."
  - "decreasekey triggers cascadingcut at least once in the dedicated structural test (T5.4)."
  - "maxdegree() = binary heap under telemetry-like workload (T5.10, --bench)."
  - "TelemetryBroker slack values match closed-form formula within 1e-9 (T5.11)."
  - "FibonacciHeap symbols appear ONLY in unit5_heap and benchmarks (INV-1 grep gate, T5.12)."

B.5.8 Unit 5 Test Matrix

| Test ID | Description |
|---|---|
| T5.1 | insert/minimum/extract_min on monotonic, reverse, and random key sequences |
| T5.2 | validate_structure() after 100k seeded mixed ops |
| T5.3 | Extract sequence equivalence vs BinaryHeapBaseline and vs std::priority_queue oracle |
| T5.4 | Cascading-cut structural test (explicit mark/cut sequence) |
| T5.5 | Degree bound invariant |
| T5.6 | Pool/free-list recycling bound |
| T5.7 | Handle invalidation semantics |
| T5.8 | ASan/LSan clean run |
| T5.9 | absorb() correctness (merged heap yields union multiset in order) |
| T5.10 | Benchmark: telemetry-like (80% decrease-key) vs routing-like (5% decrease-key) workloads |
| T5.11 | Slack formula verification against closed-form computation |
| T5.12 | INV-1 static gate: no FibonacciHeap reference outside unit5/benchmarks |
| T5.13 | Deterministic FIFO tie-break for equal slack keys |

B.6 Pipeline Orchestration (pipeline/)

B.6.1 Metrics Structure

namespace qroute::pipeline {

struct Metrics {
    // Phase 1 (Unit 1)
    double   parsetimeus          = 0.0;
    double   astbuildtime_us      = 0.0;
    uint64t astnodes              = 0;
    uint64t symboltable_entries   = 0;
    bool     thread_validation      = false;

    // Phase 4 (Unit 4)
    uint32t logicalqubits         = 0;
    uint32t physicalqubits        = 0;
    uint64t lookaheadedges        = 0;
    double   placementcosthops    = 0.0;
    double   greedyupperbound_cost= 0.0;
    uint64t nodesexpanded         = 0;
    uint64t branchespruned        = 0;
    uint32t maxstack_depth        = 0;
    bool     search_completed       = false;
    double   placementtimeus      = 0.0;

    // Phase 2 (Unit 2)
    uint64t initialgate_count       = 0;   // single + two-qubit gates
    uint64t initialsinglequbitcount = 0;
    uint64t initialtwoqubitcount  = 0;
    uint64t initialcnot_count       = 0;
    uint64t initialmeasure_count    = 0;
    uint64t initialbarrier_count    = 0;
    uint64t insertedswap_count      = 0;
    uint64t addedcnot_count         = 0;
    uint64t finalcnot_count         = 0;
    uint64t finalphysicalgatecount= 0;
    uint64t depthbefore             = 0;
    uint64t depthafter              = 0;
    double   depthexpansionpercent  = 0.0;
    double   cumulative_fidelity      = 1.0;
    double   log_infidelity           = 0.0;
    uint64t dijkstrainvocations     = 0;
    double   routingtimeus          = 0.0;

    // Phase 3 (Unit 3)
    uint32t chainlength             = 0;
    uint64t mcmoptimal_cost         = 0;
    uint64t mcmlefttoright_cost   = 0;
    uint64t mcmsavings              = 0;
    double   mcmsavingspercent      = 0.0;
    bool     mcmoverflowdetected    = false;
    std::string mcm_parenthesization;
    double   mcmplantime_us         = 0.0;

    // Phase 5 (Unit 5)
    uint64t telemetryinserts        = 0;
    uint64t telemetrydecrease_keys  = 0;
    uint64t telemetryextract_mins   = 0;
    uint64t alertsdispatched        = 0;
    double   telemetrytimeus        = 0.0;

    // Summary
    double   totalcputime_us        = 0.0;
    uint64t determinismhash         = 0;
};

} // namespace qroute::pipeline

B.6.2 Execution Engine

namespace qroute::pipeline {

struct EngineConfig {
    std::string circuit_path;
    std::string coupling_path;
    std::string out_path;
    std::string outjsonpath;         // optional
    std::string chain_path;            // optional; overrides derived chain
    std::string telemetry_path;        // optional; defaults to built-in constants
    uint32t    lookaheadwindow = 0;  // 0 == full trace
    double      placementtimelimit_ms = 1000.0;
    bool        run_benchmarks = false;
    bool        verbose = false;
};

struct RoutedCircuit {
    std::vector gates;
    std::string toqasmtext() const;
};

class ExecutionEngine {
public:
    explicit ExecutionEngine(EngineConfig cfg);
    int run();                                  // returns process exit code

    const Metrics&        metrics() const noexcept;
    const RoutedCircuit&  routed() const noexcept;
    const unit3::ContractionPlan& plan() const noexcept;
    const std::vector& alerts() const noexcept;

private:
    EngineConfig                 cfg_;
    Metrics                      metrics_;
    RoutedCircuit                routed_;
    unit3::ContractionPlan       plan_;
    std::vector    alerts_;

    unit1::ParsedProgram         program_;
    std::uniqueptr graph;
    unit4::LookaheadGraph        lookahead_;
    unit4::PlacementResult       placement_;
    unit2::PlacementMap          map_{0, 0};
    std::uniqueptr broker;

    void phase1_parse();
    void phase4_place();
    void phase2_route();
    void phase3_plan();
    void phase5_telemetry();
    void computedepthmetrics();
    void computefidelitymetrics();
    void emit_outputs();
};

} // namespace qroute::pipeline

B.6.3 Mandatory Phase Execution Order

phase1_parse()      -> ThreadedAST + AVL symbol table      [Unit 1]
phase4_place()      -> LookaheadGraph + DFS B&B            [Unit 4]
phase2_route()      -> Dijkstra + SwapSynthesizer          [Unit 2]
phase3_plan()       -> MatrixChainOptimizer (planning)     [Unit 3]
phase5_telemetry()  -> TelemetryBroker + FibonacciHeap     [Unit 5]
computedepthmetrics(); computefidelitymetrics(); emit_outputs();

This order mirrors the project methodology document (Phase 1 → 4 → 2 → 3 → 5) and is mandatory.

B.6.4 Depth and Fidelity Computation

Depth: computed by longest-path leveling over the gate dependency DAG.

depth_before: level DAG over the LOGICAL circuit (pre-routing), qubit = logical id
depth_after : level DAG over the PHYSICAL emitted stream, qubit = physical id
depthexpansionpercent = 100.0 * (depthafter - depthbefore) / max(depth_before, 1)

Fidelity (log-space accumulation, no underflow):

\[
\texttt{log\infidelity} = \sum{g \in G{physical}} -\ln(Fg)
\]

\[
\texttt{cumulative\fidelity} = e^{-\texttt{log\infidelity}}
\]

where for a synthetic SWAP component on edge \(e\): \(-\ln(Fe)\) per CNOT, i.e. \(3we\) per SWAP.

C. Directory Tree & File Layout

qroute/
├── CMakeLists.txt
├── PRD.md
├── README.md
├── .clang-format
├── .gitignore
├── cmake/
│   ├── CompilerWarnings.cmake
│   └── Sanitizers.cmake
├── scripts/
│   ├── check_invariants.py
│   ├── run_all.sh
│   └── golden_diff.py
├── include/qroute/
│   ├── common/
│   │   ├── types.hpp
│   │   ├── constants.hpp
│   │   ├── safe_arithmetic.hpp
│   │   ├── file_io.hpp
│   │   └── deterministic_time.hpp
│   ├── unit1_ast/
│   │   ├── token.hpp
│   │   ├── lexer.hpp
│   │   ├── ast_node.hpp
│   │   ├── gate_payload.hpp
│   │   ├── threaded_ast.hpp
│   │   ├── avlsymboltable.hpp
│   │   └── qasm_parser.hpp
│   ├── unit2_graph/
│   │   ├── physical_edge.hpp
│   │   ├── physicalcouplinggraph.hpp
│   │   ├── placement_map.hpp
│   │   └── swap_synthesizer.hpp
│   ├── unit3_dp/
│   │   └── matrixchainoptimizer.hpp
│   ├── unit4_bnb/
│   │   ├── lookahead_graph.hpp
│   │   └── qubitplacementbnb.hpp
│   ├── unit5_heap/
│   │   ├── fibonacci_heap.hpp
│   │   ├── binaryheapbaseline.hpp
│   │   └── telemetry_broker.hpp
│   └── pipeline/
│       ├── metrics.hpp
│       ├── execution_engine.hpp
│       └── report_writer.hpp
├── src/
│   ├── CMakeLists.txt
│   ├── common/
│   │   ├── safe_arithmetic.cpp
│   │   ├── file_io.cpp
│   │   └── deterministic_time.cpp
│   ├── unit1_ast/
│   │   ├── lexer.cpp
│   │   ├── threaded_ast.cpp
│   │   └── qasm_parser.cpp
│   ├── unit2_graph/
│   │   ├── physicalcouplinggraph.cpp
│   │   ├── placement_map.cpp
│   │   └── swap_synthesizer.cpp
│   ├── unit3_dp/
│   │   └── matrixchainoptimizer.cpp
│   ├── unit4_bnb/
│   │   ├── lookahead_graph.cpp
│   │   └── qubitplacementbnb.cpp
│   ├── unit5_heap/
│   │   ├── binaryheapbaseline.cpp
│   │   └── telemetry_broker.cpp
│   └── pipeline/
│       ├── execution_engine.cpp
│       └── report_writer.cpp
├── apps/
│   ├── CMakeLists.txt
│   └── qroute_main.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── native_assert.hpp                  # GoogleTest-free fallback macros
│   ├── testcommonsafe_arithmetic.cpp
│   ├── testunit1ast.cpp
│   ├── testunit2graph.cpp
│   ├── testunit3dp.cpp
│   ├── testunit4bnb.cpp
│   ├── testunit5heap.cpp
│   ├── testpipelinee2e.cpp
│   └── golden/
│       ├── report.golden.txt
│       └── routed.golden.qasm
├── benchmarks/
│   ├── CMakeLists.txt
│   ├── bench_traversal.cpp                # threaded vs recursive AST traversal
│   ├── bench_dijkstra.cpp                 # binary heap vs Floyd-Warshall oracle
│   ├── benchpriorityqueues.cpp          # Fibonacci vs binary heap (2 workloads)
│   ├── bench_mcm.cpp                      # DP vs left-to-right
│   └── bench_bnb.cpp                      # DFS B&B vs brute force
├── docs/
│   ├── ARCHITECTURE.md
│   ├── COMPLEXITY_ANALYSIS.md
│   └── DSAUNITMAPPING.md
└── data/
    ├── circuit.qasm
    ├── coupling.map                       # linear_5
    ├── coupling_grid9.map                 # 3x3 grid
    ├── coupling_heavyhex7.map             # heavy-hex fragment
    ├── contraction.chain
    └── telemetry.conf

C.1 CMake Target Specification

| Target | Type | Sources | Dependencies |
|---|---|---|---|
| qroute_common | STATIC | src/common/* | none |
| qrouteunit1 | STATIC | src/unit1ast/* | qroute_common |
| qrouteunit2 | STATIC | src/unit2graph/* | qroutecommon, qrouteunit1 |
| qrouteunit3 | STATIC | src/unit3dp/* | qroutecommon, qrouteunit1 |
| qrouteunit4 | STATIC | src/unit4bnb/* | qroutecommon, qrouteunit2 |
| qrouteunit5 | STATIC | src/unit5heap/* | qroute_common |
| qroute_pipeline | STATIC | src/pipeline/* | all units |
| qroute | EXECUTABLE | apps/qroutemain.cpp | qroutepipeline |
| qroutetests | EXECUTABLE | tests/* | qroutepipeline, GTest |
| qroutebench | EXECUTABLE | benchmarks/* | qroutepipeline |

CMake requirements:

cmakeminimumrequired(VERSION 3.16)
project(qroute LANGUAGES CXX VERSION 1.0.0)

set(CMAKECXXSTANDARD 17)
set(CMAKECXXSTANDARD_REQUIRED ON)
set(CMAKECXXEXTENSIONS OFF)          # strict standard C++; no GNU extensions
set(CMAKEEXPORTCOMPILE_COMMANDS ON)

option(QRouteBUILDTESTS      "Build unit tests"  ON)
option(QRouteBUILDBENCHMARKS "Build benchmarks"  ON)
option(QRoute_WERROR           "Warnings as errors" OFF)
option(QRoute_SANITIZE         "Enable ASan/UBSan"  OFF)

enable_testing()
addcustomtarget(check_invariants
    COMMAND ${CMAKE_COMMAND} -E echo "Running architectural invariant scanner..."
    COMMAND python3 ${CMAKESOURCEDIR}/scripts/check_invariants.py
    WORKINGDIRECTORY ${CMAKESOURCE_DIR})

Hard requirement: set(CMAKECXXEXTENSIONS OFF) enforces INV-4 portability.

D. CLI Interface, Input Formats & Deterministic Output Schema

D.1 CLI Specification

qroute --circuit  --coupling  --out  [options]

| Argument | Required | Default | Description |
|---|---|---|---|
| --circuit  | Yes | — | OpenQASM 2.0 input trace |
| --coupling  | Yes | — | Physical coupling map |
| --out  | Yes | — | Routed physical circuit output path |
| --bench | No | false | Run the empirical benchmark suite (Section D.6) and append the benchmark report |
| --chain  | No | derived from AST | Explicit MCM dimension chain file |
| --telemetry  | No | built-in defaults | Telemetry configuration file |
| --lookahead  | No | 0 (full trace) | Two-qubit gate window for the lookahead graph |
| --time-limit  | No | 1000.0 | B&B search time budget |
| --node-budget  | No | 0 (unlimited) | B&B expanded-node budget |
| --out-json  | No | none | Machine-readable JSON report |
| --verbose | No | false | Emit per-phase trace lines to stderr |
| --version | No | — | Print qroute 1.0.0 (R2) and exit 0 |
| --help | No | — | Print usage and exit 0 |

Argument parsing rules:
Unknown flag ⇒ stderr message + exit 2.
Missing required flag ⇒ usage message + exit 2.
--bench may be run alone (without --circuit), in which case only the benchmark report is printed and exit is 0.

D.2 Exit Codes

| Code | Meaning |
|---|---|
| 0 | Success |
| 2 | CLI argument error / unreadable input file |
| 3 | OpenQASM parse error |
| 4 | Coupling map error (bad fidelity, self-loop, duplicate edge, malformed line) |
| 5 | Placement infeasible (n > m, disconnected required pairs) |
| 6 | Internal invariant violation (assertion failure) |
| 7 | Benchmark failure |

D.3 Input Schema — coupling.map

Optional comment lines begin with '#' or '//'
 
   
...  (exactly edge_count rows)

Constraints:
u != v, 0  0
No duplicate unordered pair {u,v}
Edges may appear in any order; loader sorts adjacency ascending by to

data/coupling.map (mandatory content):

Q-Route reference topology: linear chain, 5 physical qubits
5 4
0 1 0.9990 120.0
1 2 0.9980 135.0
2 3 0.9990 118.0
3 4 0.9980 140.0

data/coupling_grid9.map (mandatory test topology):

3x3 grid
9 12
0 1 0.9990 120.0
1 2 0.9985 125.0
3 4 0.9990 118.0
4 5 0.9980 140.0
6 7 0.9988 122.0
7 8 0.9992 115.0
0 3 0.9987 128.0
1 4 0.9991 119.0
2 5 0.9983 133.0
3 6 0.9989 121.0
4 7 0.9993 114.0
5 8 0.9986 127.0

data/coupling_heavyhex7.map (mandatory test topology):

heavy-hex fragment, 7 physical qubits
7 7
0 1 0.9988 124.0
1 2 0.9991 117.0
1 3 0.9985 130.0
3 4 0.9990 120.0
3 5 0.9982 138.0
5 6 0.9989 123.0
2 6 0.9984 132.0

D.4 Input Schema — circuit.qasm

Restricted OpenQASM 2.0 subset per B.1.3.

data/circuit.qasm (mandatory content):

OPENQASM 2.0;
include "qelib1.inc";

qreg q[5];
creg c[5];

h q[0];
rx(0.12) q[0];
ry(0.45) q[1];
rz(pi/4) q[2];

barrier q[0],q[1],q[2],q[3],q[4];

cx q[0],q[4];
cx q[1],q[3];
cx q[0],q[3];
cx q[1],q[4];

measure q[0] -> c[0];
measure q[1] -> c[1];
measure q[2] -> c[2];
measure q[3] -> c[3];
measure q[4] -> c[4];

Derived counting contract for this exact file:

| Metric | Value |
|---|---|
| ast_nodes | 18 (2 header/include + 2 declarations + 14 instructions) |
| initialsinglequbit_count | 4 (h, rx, ry, rz) |
| initialtwoqubitcount / initialcnot_count | 4 |
| initialgatecount | 8 (single + two-qubit) |
| initialbarriercount | 1 |
| initialmeasurecount | 5 |
| lookahead_edges | 4 distinct logical pairs: (0,4), (1,3), (0,3), (1,4) |
| logicalqubits / physicalqubits | 5 / 5 |

data/contraction.chain (mandatory content):

k matrices, then k+1 dimensions
4
10 20 30 40 30

Expected: mcmoptimalcost = 26000.

data/telemetry.conf (mandatory content):

key = value
t1_us              = 50.0
t2_us              = 70.0
fidelity_threshold = 0.90
alertslackus     = 5.0
singlequbitns    = 35.0
twoqubitns       = 300.0
measure_ns         = 800.0

D.5 Deterministic stdout Report Schema

ReportWriter::write_stdout(const Metrics&) must emit exactly this layout, field order, labels, alignment (label field padded to 26 characters,  :  separator), and precision.

================================================================================
Q-Route Execution Report
================================================================================
pipeline_revision       : R2
circuit_file            : 
coupling_file           : 

[Phase 1] Unit 1 - Threaded AST Ingestion
parsetimeus           : 
astbuildtime_us       : 
ast_nodes               : 
symboltableentries    : 
thread_validation       : PASS|FAIL

[Phase 4] Unit 4 - DFS Branch & Bound Placement
logical_qubits          : 
physical_qubits         : 
lookahead_edges         : 
greedyupperbound_cost : 
placementcosthops     : 
nodes_expanded          : 
branches_pruned         : 
maxstackdepth         : 
search_completed        : true|false
placementtimeus       : 

[Phase 2] Unit 2 - Binary-Heap Dijkstra Routing
initialgatecount      : 
initialsinglequbit    : 
initialtwoqubit       : 
initialcnotcount      : 
initialmeasurecount   : 
initialbarriercount   : 
dijkstra_invocations    : 
insertedswapcount     : 
addedcnotcount        : 
finalcnotcount        : 
finalphysicalgates    : 
depth_before            : 
depth_after             : 
depthexpansionpercent : 
cumulative_fidelity     : 
log_infidelity          : 
routingtimeus         : 

[Phase 3] Unit 3 - MCM Contraction Planning
chain_length            : 
mcmoptimalcost        : 
mcmlefttorightcost  : 
mcm_savings             : 
mcmsavingspercent     : 
mcmoverflowdetected   : true|false
mcm_parenthesization    : 
mcmplantime_us        : 

[Phase 5] Unit 5 - Fibonacci Heap Telemetry Broker
telemetry_inserts       : 
telemetrydecreasekeys : 
telemetryextractmins  : 
alerts_dispatched       : 
telemetrytimeus       : 

[Summary]
totalcputime_us       : 
determinism_hash        : 
================================================================================

Formatting rules (mandatory):

| Rule | Specification |
|---|---|
| Label column | Left-aligned, padded to width 26, followed by :  |
| Durations | std::fixed 
// coupling          : 
// pipeline_revision : R2
// logical_qubits    : 
// physical_qubits   : 
// inserted_swaps    : 
// added_cnots       : 
// cumulative_fidelity : 
OPENQASM 2.0;
qreg p[];
creg c[];
cx p[1],p[2];      // swap-component 1/3 (logical SWAP p1p2)
cx p[2],p[1];      // swap-component 2/3
cx p[1],p[2];      // swap-component 3/3
cx p[3],p[4];      // routed CX (logical cx q[0],q[4])
...

Requirements:
Only physically adjacent two-qubit gates appear. Assertion enforced at write time.
Every SWAP is fully decomposed into exactly 3 cx lines.
Comments are stable and deterministic (no addresses, no timestamps).

D.7 --out-json Schema

{
  "schema_version": "1.0",
  "pipeline_revision": "R2",
  "inputs": { "circuit": "...", "coupling": "...", "lookahead_window": 0 },
  "metrics": { "": 0 },
  "placement": { "logicaltophysical": [0,1,2,3,4] },
  "contraction_plan": {
    "dimensions": [10,20,30,40,30],
    "optimal_cost": 26000,
    "lefttoright_cost": 41000,
    "savings": 15000,
    "parenthesization": "((A1(A2A3))A4)",
    "merge_order": [[1,2],[0,3]],
    "overflow_detected": false
  },
  "alerts": [ { "qubit": 3, "slackus": 4.21, "elapsedus": 12.4, "gate_index": 17 } ],
  "timings_us": { "parse": 0.0, "placement": 0.0, "routing": 0.0, "mcm": 0.0, "telemetry": 0.0, "total": 0.0 },
  "determinism_hash": "0000000000000000"
}

JSON must be emitted with 2-space indentation, keys in the exact order above, and no trailing whitespace.

D.8 --bench Benchmark Report Schema

================================================================================
Q-Route Benchmark Suite (--bench)
================================================================================
[B1] AST Traversal: Threaded vs Recursive
nodes,threadedus,recursiveus,speedup,maxrecursiondepth,auxstackbytes
1000,...
1000000,...

[B2] Routing: Binary-Heap Dijkstra vs Floyd-Warshall Oracle
topology,vertices,edges,dijkstraus,floydus,match,maxabsdelta
linear_5,5,4,...,EXACT,0.000000000
grid_3x3,9,12,...,EXACT,0.000000000
heavyhex_7,7,7,...,EXACT,0.000000000

[B3] Priority Queue: Fibonacci vs Binary Heap
workload,ops,insertpct,decreasekeypct,extractpct,fibus,binaryus,fibopspersec,binaryopspersec,winner
routing_like,200000,70,5,25,...
telemetry_like,200000,20,75,5,...

[B4] MCM: DP vs Left-to-Right
chainlength,optimalcost,lefttorightcost,savings,savingspercent,dptimeus
4,26000,41000,15000,36.585366,...
8,...
16,...

[B5] Placement: DFS Branch & Bound vs Brute Force
m,n,bruteforcenodes,bnbnodesexpanded,bnbpruned,reductionratio,bnbcost,brutecost,match,maxstackdepth
4,4,...
5,4,...
6,5,...

[B6] Memory & Complexity Scaling
module,inputsize,peakstackbytes,heapallocationsduringhotloop,observedorder
unit1_ast,1000000,...,O(n)
unit4_bnb,n=16,...,O(n)
================================================================================

--bench must also write build/bench_results.csv with all [B1]–[B5] rows in flat CSV form.

Expected empirical conclusions the benchmark must be able to demonstrate:

| Benchmark | Expected Result |
|---|---|
| B1 | Threaded traversal has maxrecursiondepth = 0, auxstackbytes ≈ 0; recursive variant grows with tree height |
| B2 | Dijkstra matches oracle exactly (maxabsdelta  0 for all non-trivial chains |
| B5 | bnbcost == brutecost (optimality) and reduction_ratio > 1 |
| B6 | heapallocationsduringhotloop == 0 for Unit 4 |

E. Phased Implementation Roadmap for agy

Loop instruction: execute Task 1 → 6 in strict order. Each task ends with a hard gate. Do not proceed on gate: FAIL.

TASK 1 — Project Scaffold, CMake, Common Types

task_manifest:
  id: "TASK-1"
  title: "Scaffold CMake, directory tree, and common data types"
  depends_on: []
  estimate: "small"

filestocreate:
  - CMakeLists.txt
  - cmake/CompilerWarnings.cmake
  - cmake/Sanitizers.cmake
  - .clang-format
  - .gitignore
  - README.md
  - scripts/check_invariants.py
  - scripts/run_all.sh
  - include/qroute/common/types.hpp
  - include/qroute/common/constants.hpp
  - include/qroute/common/safe_arithmetic.hpp
  - include/qroute/common/file_io.hpp
  - include/qroute/common/deterministic_time.hpp
  - src/common/safe_arithmetic.cpp
  - src/common/file_io.cpp
  - src/common/deterministic_time.cpp
  - src/CMakeLists.txt
  - apps/CMakeLists.txt
  - apps/qroute_main.cpp            # stub: --help/--version only, exit 0
  - tests/CMakeLists.txt
  - tests/native_assert.hpp
  - tests/testcommonsafe_arithmetic.cpp
  - benchmarks/CMakeLists.txt
  - data/circuit.qasm
  - data/coupling.map
  - data/coupling_grid9.map
  - data/coupling_heavyhex7.map
  - data/contraction.chain
  - data/telemetry.conf
  - docs/ARCHITECTURE.md            # pipeline diagram + invariant list
  - docs/DSAUNITMAPPING.md        # unit -> module -> syllabus mapping table

implementation_steps:
  - "1. Create the full directory tree exactly as specified in Section C."
  - "2. Write root CMakeLists.txt: project(), C++17, CMAKECXXEXTENSIONS OFF, options, enabletesting(), addsubdirectory for src/apps/tests/benchmarks, addcustomtarget(check_invariants)."
  - "3. Write cmake/CompilerWarnings.cmake exposing targetenablewarnings(tgt) applying -Wall -Wextra -Wpedantic (GCC/Clang) or /W4 (MSVC), plus /permissive- on MSVC; honor QRoute_WERROR."
  - "4. Write cmake/Sanitizers.cmake: when QRoute_SANITIZE=ON add -fsanitize=address,undefined -fno-omit-frame-pointer (GCC/Clang only)."
  - "5. Implement common/types.hpp, constants.hpp exactly as in Section B.0."
  - "6. Implement SafeArithmetic with multiplyu64, addu64, saturatedtripleproductadd, fnv1a64, fnv1a64str. NO 128-bit types [INV-4]."
  - "7. Implement fileio (readtextfile, readlines with 1-based numbering, writetextfile with binary mode on Windows, splittokens, stripcomment)."
  - "8. Implement deterministictime (ScopedTimerUs, nowus)."
  - "9. Write scripts/check_invariants.py implementing ALL rules from Section 0.3; exit non-zero on violation."
  - "10. Write scripts/runall.sh: cmake configure -> build -> ctest -> checkinvariants -> run CLI on data/ samples."
  - "11. Write data/ files with EXACT content from Sections D.3 and D.4."
  - "12. Write apps/qroute_main.cpp stub: install std::locale::classic(), parse --help/--version, otherwise print 'TASK-1 stub' and return 0."
  - "13. Write docs/ARCHITECTURE.md and docs/DSAUNITMAPPING.md."

teststocreate:
  - T0.1: SafeArithmetic::multiply_u64 boundary (0, 1, MAX, MAX/2, overflow cases)
  - T0.2: SafeArithmetic::add_u64 boundary and saturation
  - T0.3: saturatedtripleproduct_add chained overflow propagation
  - T0.4: grep-based assertion that no source contains 'int128' [INV-4]
  - T0.5: fnv1a64 known-answer tests (empty string, 'a', 'foobar')
  - T0.6: file_io round-trip write/read byte equality
  - T0.7: read_lines 1-based numbering and CRLF stripping
  - T0.8: data/ files exist, are non-empty, and parse their header lines correctly

acceptance_criteria:
  - "cmake -S . -B build && cmake --build build succeeds with zero warnings."
  - "ctest --test-dir build --output-on-failure passes 100%."
  - "python3 scripts/check_invariants.py exits 0 and prints ALL INVARIANTS SATISFIED."
  - "All Section D.3/D.4 data files exist with exact specified content."
  - "CMAKECXXEXTENSIONS is OFF (verified via compile_commands.json inspection: no -std=gnu++17)."

verify_commands:
  - "cmake -S . -B build -DQRouteBUILDTESTS=ON -DQRouteBUILDBENCHMARKS=ON"
  - "cmake --build build -j"
  - "ctest --test-dir build --output-on-failure"
  - "python3 scripts/check_invariants.py"

gate: "PASS only if all acceptance_criteria are true."

TASK 2 — Unit 1: Lexer, Parser, ThreadedAST, AVL Symbol Table

task_manifest:
  id: "TASK-2"
  title: "Unit 1 - AST, Lexer/Parser, AVL symbol table, threaded in-order traversal"
  depends_on: ["TASK-1"]

filestocreate:
  - include/qroute/unit1_ast/token.hpp
  - include/qroute/unit1_ast/lexer.hpp
  - include/qroute/unit1ast/astnode.hpp
  - include/qroute/unit1ast/gatepayload.hpp
  - include/qroute/unit1ast/threadedast.hpp
  - include/qroute/unit1ast/avlsymbol_table.hpp
  - include/qroute/unit1ast/qasmparser.hpp
  - src/unit1_ast/lexer.cpp
  - src/unit1ast/threadedast.cpp
  - src/unit1ast/qasmparser.cpp
  - tests/testunit1ast.cpp

implementation_steps:
  - "1. Implement TokenType/Token and Lexer per B.1.2 (all rules L-1..L-7)."
  - "2. Implement ASTNode with staticassert(sizeof == 24) and GatePayload with staticassert(sizeof == 64) per B.1.4."
  - "3. Implement ThreadedAST: reserve, append, buildbalanced (midpoint recursion at BUILD time only), buildfromsequence, finalizethreads, leftmost, next, prev, traverse, inorderreference, validatethreads, height, is_balanced."
  - "4. finalizethreads must compute the inorder sequence once (may use a temporary vector), then set leftthread/right_thread + indices for every absent child. Temporary vector must be released (swap-with-empty) after threading."
  - "5. Implement AvlSymbolTable template per B.1.6 with LL/RR/LR/RL rotations, per-node height, uniqueptr pool ownership, isbalanced(), inorder()."
  - "6. Implement QasmParser per B.1.3 grammar subset and all rules P-1..P-12, including the recursive-descent expression evaluator (pi, unary +/-, + - * /, parentheses)."
  - "7. Parser must register qreg/creg into the AVL symbol table with flattened base_index, and resolve qarg/carg through it."
  - "8. Wire qroute_unit1 static library into src/CMakeLists.txt and tests/CMakeLists.txt."
  - "9. Extend apps/qroutemain.cpp to accept --circuit and print 'astnodes' and 'symboltableentries' only (full report arrives in TASK-6)."

teststocreate:
  - T1.1 .. T1.11 (full matrix per Section B.1.8)

acceptance_criteria:
  - "static_asserts on ASTNode (24) and GatePayload (64) compile."
  - "data/circuit.qasm parses with zero errors; astnodes == 18; symboltable_entries == 2 (q, c)."
  - "T1.3 traversal order == ascending seq == source order."
  - "T1.4 validate_threads() true on empty / 1-node / 1000-node trees."
  - "T1.5 traversal uses zero auxiliary stack containers."
  - "T1.7 AVL 10k sorted inserts: is_balanced() true and height , ..., std::greater<>> [INV-1]. Lazy deletion, epsilon comparisons, deterministic tie-break."
  - "7. Implement shortest_path() with prev reconstruction + adjacency assertion on every consecutive pair; empty vector when unreachable."
  - "8. Implement computeallpairs(): BFS from every vertex for hopmatrix (uint16t, UINT16MAX for unreachable) and Dijkstra from every vertex for costmatrix (row-major u*n_+v)."
  - "9. Implement hops() and weighteddistance() reading the cached matrices; auto-call computeall_pairs() lazily on first query."
  - "10. Implement mstprim() (binary heap) and mstkruskal() (sort edges + union-find with path compression and union by rank); expose msttotalweight()."
  - "11. Implement bellmanford() and floydwarshall() as VALIDATION ORACLES ONLY, plus validateagainstoracles(tol)."
  - "12. Implement PlacementMap per B.2.5 with swapphysical maintaining both directions and validatebijection()."
  - "13. Implement SwapSynthesizer per B.2.6 including decomposeswap emitting exactly 3 EmittedGate CX records, swapedgesforpath, and the route() algorithm with mapping mutation and hard adjacency assertion."
  - "14. Wire qroute_unit2 into the build."

teststocreate:
  - T2.1 .. T2.13 (full matrix per Section B.2.8)

acceptance_criteria:
  - "static_assert(sizeof(PhysicalEdge) == 32) compiles."
  - "All three data/*.map topologies load, sort, and report is_connected() == true."
  - "T2.4/T2.5: Dijkstra matches Floyd-Warshall AND Bellman-Ford on linear5, grid3x3, heavyhex_7."
  - "T2.6: every shortest_path result is a valid adjacent chain; unreachable returns empty."
  - "T2.8: Prim and Kruskal total weights are identical on all three topologies."
  - "T2.9: 20 repeated Dijkstra runs produce byte-identical dist and prev arrays."
  - "T2.10: SwapSynthesizer emits only adjacent two-qubit gates across a 20-gate trace on grid_3x3."
  - "T2.11: PlacementMap::validate_bijection() holds after every swap across the full trace."
  - "T2.12: addedcnotcount == 3 * swap_count always."
  - "T2.13 [INV-1]: zero 'FibonacciHeap' occurrences in unit2 sources; scanner exits 0."
  - "All prior tests still pass."

verify_commands:
  - "cmake --build build -j"
  - "ctest --test-dir build --output-on-failure -R unit2"
  - "ctest --test-dir build --output-on-failure"
  - "python3 scripts/check_invariants.py"
  - "grep -rn FibonacciHeap include/qroute/unit2graph src/unit2graph || echo CLEAN"

gate: "PASS only if all acceptance_criteria are true."

TASK 4 — Unit 4: Lookahead Graph, Greedy Bound, DFS Branch & Bound

task_manifest:
  id: "TASK-4"
  title: "Unit 4 - Lookahead interaction graph, greedy upper bound, DFS Branch & Bound placement"
  depends_on: ["TASK-3"]

filestocreate:
  - include/qroute/unit4bnb/lookaheadgraph.hpp
  - include/qroute/unit4bnb/qubitplacement_bnb.hpp
  - src/unit4bnb/lookaheadgraph.cpp
  - src/unit4bnb/qubitplacement_bnb.cpp
  - tests/testunit4bnb.cpp

implementation_steps:
  - "1. Implement LookaheadGraph per B.4.2: build() traverses the ThreadedAST via traverse() collecting two-qubit gate operand pairs; honors the lookahead window (0 == all); accumulates alpha_ij as interaction counts; symmetric matrix with zero diagonal."
  - "2. Implement degree(), degreesortedorder() (degree DESC, id ASC), edges() sorted ascending by (i,j)."
  - "3. Implement PlacementConfig / PlacementResult structs per B.4.3."
  - "4. Implement QubitPlacementBAndB::precomputeboundstructures(): for every physical p build sorteddistfrom[p] as ascending (distance, vertex) pairs from the graph's cached hop or weighted matrix; compute minpairdistance = minimum distance over all distinct physical pairs."
  - "5. Implement greedyupperbound(): iterate variableorder, assign each logical qubit to the unused physical qubit minimizing incremental cost; store the resulting assignment as the initial bestassignment and bestcost (register B). Set feasible accordingly."
  - "6. Implement assign() / undoassign() as IN-PLACE mutations of currentl2p and currentused_. Zero allocation."
  - "7. Implement assignincrementalcost(l, p): sum over previously assigned logical qubits l2 of alpha(l,l2) * distance(p, pi(l2))."
  - "8. Implement mindistancetounused(p): scan sorteddistfrom[p] and return the first entry whose vertex is currently unused."
  - "9. Implement lowerbound(assignedcost) per B.4.5 covering all four deltaij cases; iterate cfg.lookahead.edges() in ascending order for determinism."
  - "10. Implement budgetexhausted(): checks nodebudget and steadyclock deadline against timelimit_ms."
  - "11. Implement solve() EXACTLY per the mandatory DFS loop shape in B.4.7: explicit std::vector stack reserved once to n+1, per-frame nextphysical resume cursor, frame-owned assignment undo on pop, B tightening on complete assignments, counters for nodesexpanded/branchespruned/maxstackdepth."
  - "12. Add runtime assertion: assert(maxstackdepth  m => feasible=false; disconnected graph making some alpha_ij edge unreachable => cost kInfinity, report infeasible if no finite assignment exists."
  - "14. Wire qroute_unit4 into the build."

teststocreate:
  - T4.1 .. T4.14 (full matrix per Section B.4.10)
  - "Include a brute-force reference implementation INSIDE the test file only (std::next_permutation), never in src/."

acceptance_criteria:
  - "T4.3: B&B cost == brute-force optimum on (4,4), (5,4), (6,5), (6,6)."
  - "T4.5: lower bound admissibility verified against exhaustive completion costs."
  - "T4.7: maxstackdepth()  0 on the reference dataset; nodesexpanded = 2, every dimension > 0, else std::invalid_argument."
  - "3. Allocate m and s as flat 1D std::vector of size n*n (row-major). Provide m(i,j), s(i,j), mat(i,j), sat(i,j) index helpers."
  - "4. Implement rundp() EXACTLY per B.3.4: chain-length ascending loops, SafeArithmetic::addu64 for sub-costs, SafeArithmetic::saturatedtripleproduct_add for p[i]p[r+1]p[j+1] + sub, strict ' 0)."
  - "10. Implement derivechainfromast() per B.3.6 with greedylayer_partition and the kMaxMcmDimShift clamp."
  - "11. NO quantum simulation code of any kind [INV-3]. NO 128-bit arithmetic [INV-4]."
  - "12. Wire qrouteunit3 into the build; add benchmcm target."

teststocreate:
  - T3.1 .. T3.11 (full matrix per Section B.3.8)
  - "Include a brute-force parenthesization enumerator INSIDE the test file only."

acceptance_criteria:
  - "T3.2: chain {10,20,30,40,30} => optimal_cost == 26000."
  - "T3.3: chain {30,35,15,5,10,20,25} => optimal_cost == 15125."
  - "T3.4: DP == brute force on 200 seeded random chains with k  optimal."
  - "T3.8 [INV-3]: grep finds zero occurrences of std::complex/blas/lapack/Eigen/state_vector/amplitude in unit3."
  - "T3.9: costtable/splittable are contiguous row-major (data() pointer arithmetic verified)."
  - "T3.10: derivechainfrom_ast deterministic across 10 runs."
  - "T3.11: k=20 -> k=40 wall-time ratio within [4x, 16x] consistent with O(k^3)."
  - "data/contraction.chain loads and yields optimal_cost == 26000."
  - "All prior tests still pass; scanner exits 0."

verify_commands:
  - "cmake --build build -j"
  - "ctest --test-dir build --output-on-failure -R unit3"
  - "ctest --test-dir build --output-on-failure"
  - "python3 scripts/check_invariants.py"
  - "grep -rniE 'std::complex|blas|lapack|eigen|statevector|amplitude' include/qroute/unit3dp src/unit3_dp || echo CLEAN"

gate: "PASS only if all acceptance_criteria are true."

TASK 6 — Unit 5 + End-to-End Pipeline + CLI + Benchmarks

task_manifest:
  id: "TASK-6"
  title: "Unit 5 - Fibonacci heap, TelemetryBroker, full pipeline, CLI, benchmarks, golden files"
  depends_on: ["TASK-4", "TASK-5"]

filestocreate:
  - include/qroute/unit5heap/fibonacciheap.hpp
  - include/qroute/unit5heap/binaryheap_baseline.hpp
  - include/qroute/unit5heap/telemetrybroker.hpp
  - src/unit5heap/binaryheap_baseline.cpp
  - src/unit5heap/telemetrybroker.cpp
  - include/qroute/pipeline/metrics.hpp
  - include/qroute/pipeline/execution_engine.hpp
  - include/qroute/pipeline/report_writer.hpp
  - src/pipeline/execution_engine.cpp
  - src/pipeline/report_writer.cpp
  - apps/qroute_main.cpp            # FULL implementation replacing the TASK-1 stub
  - tests/testunit5heap.cpp
  - tests/testpipelinee2e.cpp
  - benchmarks/bench_traversal.cpp
  - benchmarks/bench_dijkstra.cpp
  - benchmarks/benchpriorityqueues.cpp
  - benchmarks/bench_bnb.cpp
  - scripts/golden_diff.py
  - tests/golden/report.golden.txt
  - tests/golden/routed.golden.qasm
  - docs/COMPLEXITY_ANALYSIS.md

implementation_steps:
  - "1. Implement FibonacciHeap as a header-only template per B.5.2 and all rules FH-1..FH-13: std::deque pool, freelist recycling, reusable degreetable, circular doubly-linked root and child lists, lazy insert, extractmin with child splicing + consolidate + link, decreasekey with cut + cascadingcut + mark semantics, absorb() by copy-in re-insertion, validate_structure(), Stats counters."
  - "2. Enforce maxdegree bound floor(logphi(n)) + 1 inside validate_structure(); phi = (1+sqrt(5))/2."
  - "3. Implement BinaryHeapBaseline (indexed binary heap with decreasekey) as benchmark baseline + test oracle. It MUST NOT be referenced from unit2graph."
  - "4. Implement TelemetryConfig::loadfromfile for telemetry.conf (D.4) with validation and defaults."
  - "5. Implement TelemetryBroker per B.5.5/B.5.6: per-qubit elapsedns accumulation, hazardrate = 1/T1 + 1/T2, criticaltimeus = -ln(F*)/hazardrate, slackus = criticaltime - elapsedus, heap key = (slackus, insertionseq) for deterministic FIFO tie-break, updateslack performing insert-or-decreasekey, draincritical returning alerts sorted ascending by (slackus, qubit)."
  - "6. Implement pipeline/metrics.hpp exactly as B.6.1 (field names and order are contractual)."
  - "7. Implement ExecutionEngine phases in the MANDATORY order B.6.3: phase1parse, phase4place, phase2route, phase3plan, phase5telemetry, then computedepthmetrics, computefidelitymetrics, emitoutputs."
  - "8. phase2route: traverse the ThreadedAST with traverse(); for each single-qubit gate emit one EmittedGate; for each two-qubit gate call SwapSynthesizer::route; for each measure emit a classical record; accumulate loginfidelity in log space; after each emitted gate call broker->ongate_executed for every touched physical qubit with the configured duration."
  - "9. computedepthmetrics: build the pre-routing logical DAG and post-routing physical DAG, level both by longest-path, populate depthbefore/depthafter/depthexpansionpercent."
  - "10. Implement ReportWriter::writestdout per the EXACT schema in D.5, including label padding to 26 chars, precision rules, SATURATED handling, and determinismhash computed by FNV-1a over the exact field list in D.5 excluding timings."
  - "11. Implement ReportWriter::writeroutedqasm per D.6 (with the adjacency assertion at write time) and write_json per D.7."
  - "12. Implement apps/qroute_main.cpp: install std::locale::classic(); parse all Section D.1 flags with validation; dispatch to ExecutionEngine::run(); when --bench is present run the benchmark suite; map failures to the Section D.2 exit codes; print errors to stderr only."
  - "13. Implement all benchmark executables producing the exact Section D.8 schemas and writing build/bench_results.csv."
  - "14. Implement scripts/goldendiff.py: compares generated report against tests/golden/report.golden.txt while MASKING all timing fields (*timeus, totalcputimeus) and comparing determinism_hash exactly."
  - "15. Generate golden files from a verified run and commit them under tests/golden/."
  - "16. Write docs/COMPLEXITY_ANALYSIS.md containing the theoretical vs empirical table for all five units."
  - "17. Add testpipelinee2e.cpp wiring the golden-file check into ctest."

teststocreate:
  - T5.1 .. T5.13 (full matrix per Section B.5.8)
  - T6.1: End-to-end run on data/circuit.qasm + data/coupling.map exits 0
  - T6.2: Every emitted two-qubit gate in routed output is physically adjacent
  - T6.3: addedcnotcount == 3 * insertedswapcount in the e2e run
  - T6.4: Two consecutive runs produce identical determinism_hash
  - T6.5: Golden report diff passes with timing fields masked
  - T6.6: Golden routed circuit byte-equality
  - T6.7: CLI exit codes 2/3/4/5 exercised via malformed inputs
  - T6.8: --out-json validates against the D.7 key set and ordering
  - T6.9: --bench completes, writes benchresults.csv, and B3 reports binary heap winning routinglike and Fibonacci winning telemetry_like
  - T6.10: [INV-1] grep gate across the whole tree: FibonacciHeap appears ONLY in unit5_heap, benchmarks, tests, docs
  - T6.11: ASan/LSan/UBSan clean full-suite run (QRoute_SANITIZE=ON)
  - T6.12: Memory-growth guard: running the pipeline on a 100k-instruction synthetic circuit stays under 512 MB RSS

acceptance_criteria:
  - "All Unit 5 tests pass, including cascading-cut structural test and oracle equivalence."
  - "T6.4/T6.5/T6.6: determinism and golden files verified."
  - "T6.2/T6.3: routing legality and SWAP accounting invariants hold end-to-end."
  - "T6.9: benchmark suite produces all five [B1]-[B5] sections with the expected winners."
  - "T6.10 [INV-1]: scanner + grep confirm zero FibonacciHeap usage in Unit 2."
  - "T6.11: sanitizer-clean."
  - "qroute --circuit data/circuit.qasm --coupling data/coupling.map --out build/routed.qasm exits 0 and prints the complete D.5 report."
  - "qroute --bench exits 0."
  - "ALL tests from TASK-1 through TASK-6 pass; scanner exits 0; zero warnings."

verify_commands:
  - "cmake -S . -B build -DQRouteBUILDTESTS=ON -DQRouteBUILDBENCHMARKS=ON"
  - "cmake --build build -j"
  - "ctest --test-dir build --output-on-failure"
  - "python3 scripts/check_invariants.py"
  - "./build/apps/qroute --circuit data/circuit.qasm --coupling data/coupling.map --out build/routed.qasm --out-json build/report.json"
  - "./build/apps/qroute --bench"
  - "python3 scripts/goldendiff.py build/reportstdout.txt tests/golden/report.golden.txt"
  - "cmake -S . -B build-san -DQRoute_SANITIZE=ON && cmake --build build-san -j && ctest --test-dir build-san --output-on-failure"
  - "bash scripts/run_all.sh"

gate: "PASS only if all acceptance_criteria are true. On PASS the project is COMPLETE."

F. Global Quality Gates

F.1 Verification Command Sequence (Run After Every Task)

cmake -S . -B build -DQRouteBUILDTESTS=ON -DQRouteBUILDBENCHMARKS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
python3 scripts/check_invariants.py
./build/apps/qroute --circuit data/circuit.qasm \
                    --coupling data/coupling.map \
                    --out build/routed.qasm

F.2 Warning / Sanitizer Policy

| Configuration | Requirement |
|---|---|
| Default build | Zero warnings under -Wall -Wextra -Wpedantic / /W4 |
| QRoute_WERROR=ON | Any warning fails the build |
| QRoute_SANITIZE=ON | ASan + UBSan clean across the full test suite (Task 6 gate) |
| MSVC | /permissive-, no POSIX-only APIs, no int128 |

F.3 Performance Budgets

| Metric | Budget | Verified By |
|---|---|---|
| 1e6-node threaded AST traversal |  m | More logical than physical qubits | PlacementResult.feasible = false; exit code 5 |
| Invalid fidelity | F  1 | Throw at load; exit code 4 |
| Duplicate / self-loop edge | Malformed coupling.map | Throw at load with line number; exit code 4 |
| Unreachable routing target | Empty shortest_path | Throw RoutingError; exit code 6 (invariant: placement should have prevented this) |
| DP overflow | Huge dimensions | Saturate to kMaxU64, set overflow_detected, print SATURATED; never UB |
| B&B time-limit expiry | Deadline exceeded | Return best-so-far, completed = false, still a valid injective assignment |
| Empty circuit | Zero instructions | Valid: all metrics zero, depthbefore = depthafter = 0, depthexpansionpercent = 0.0, cumulative_fidelity = 1.0 |
| Chain length < 2 | Degenerate MCM | Return optimal_cost = 0, parenthesization = "A1", no crash |
| Deep AST | 1e6+ nodes | Threaded traversal only; no recursion anywhere in the hot path |
| Fibonacci handle reuse | Handle used after extractmin | containshandle() returns false; broker clears its handle slot immediately after extraction |
| Locale-dependent formatting | Non-C locale on host | std::locale::classic() installed at main() entry — mandatory |
| Windows line endings | CRLF input files | read_lines strips \r; output written in binary mode with \n |

F.5 Course Rubric Alignment (Documentation Deliverables)

docs/ must contain material mapping directly onto the 17-section report structure and the 40-mark rubric:

| Rubric Module (Weight) | Q-Route Evidence Location |
|---|---|
| Module I — Tree-Based Data Management (20%, 8 marks) | docs/DSAUNITMAPPING.md §1; Unit 1 tests T1.1–T1.11; benchmark B1; AVL vs Threaded vs BST vs Binary Heap comparison table |
| Module II — Graph Modeling & Network Optimization (20%, 8 marks) | §2; adjacency list vs matrix analysis; BFS/DFS/Dijkstra/Bellman-Ford/Floyd-Warshall/Prim/Kruskal implementations; benchmark B2 |
| Module III — Dynamic Programming (15%, 6 marks) | §3; MCM recurrence, DP state tables, DP vs left-to-right comparison; benchmark B4 |
| Module IV — Backtracking & Branch & Bound (15%, 6 marks) | §4; state-space tree, bounding function proof, search tree statistics; benchmark B5 |
| Module V — Advanced Data Structures (15%, 6 marks) | §5; Red-Black/B/B+/Binomial/Fibonacci comparative justification; benchmark B3 |
| Documentation, Complexity Analysis & Presentation (15%, 6 marks) | docs/COMPLEXITY_ANALYSIS.md, docs/ARCHITECTURE.md, golden report output, all complexity tables |

G. Glossary

| Term | Definition |
|---|---|
| QNN | Quantum Neural Network; parameterized quantum circuit used as an ML model |
| NISQ | Noisy Intermediate-Scale Quantum hardware regime |
| Coupling graph | Undirected graph of physically adjacent qubit pairs on a QPU |
| Logical qubit | Abstract program qubit (q[i] in QASM) |
| Physical qubit | Hardware qubit (vertex of the coupling graph) |
| SWAP | Two-qubit permutation gate; decomposes into exactly 3 CX gates |
| Fidelity \(F_{ij}\) | Success probability of a two-qubit gate on edge \((i,j)\), \(F \in (0,1]\) |
| Additive weight \(w{ij}\) | \(-\ln(F{ij})\); converts multiplicative fidelity into a Dijkstra-compatible metric |
| Lookahead graph \(G_L\) | Weighted logical interaction graph derived from upcoming two-qubit gates |
| \(B\) | Global upper-bound register in DFS Branch & Bound |
| \(LB\) | Admissible lower bound at a partial assignment node |
| MCM | Matrix Chain Multiplication dynamic programming problem |
| \(m[i,j]\) / \(s[i,j]\) | MCM optimal-cost table / optimal-split table |
| Slack \(s_q\) | Remaining time before physical qubit \(q\) breaches the fidelity threshold |
| Threaded binary tree | Binary tree where null child pointers are replaced by inorder predecessor/successor links |
| R2 | Revision 2 of this architecture, containing the four binding invariants INV-1..INV-4 |

H. Final Agent Instruction

final_instruction:
  - "Begin with TASK-1. Do not skip tasks. Do not reorder tasks."
  - "Treat INV-1, INV-2, INV-3, INV-4 as absolute constraints that override any optimization instinct."
  - "Every public API must match the signatures in Section B exactly; tests depend on them."
  - "Every size contract static_assert must compile; do not weaken or remove them."
  - "Every report field name, order, label width, and precision in Section D.5 is contractual; do not reformat."
  - "On completion of TASK-6 with gate PASS, run scripts/runall.sh and emit the final taskcompletion manifest."
  - "If a requirement in this PRD is ambiguous, choose the interpretation that is MORE deterministic, MORE memory-bounded, and MORE portable, and record the decision in docs/ARCHITECTURE.md under 'Design Decisions'."