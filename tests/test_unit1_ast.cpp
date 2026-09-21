#include "native_assert.hpp"
#include "qroute/common/constants.hpp"
#include "qroute/common/file_io.hpp"
#include "qroute/common/types.hpp"
#include "qroute/unit1_ast/ast_node.hpp"
#include "qroute/unit1_ast/avl_symbol_table.hpp"
#include "qroute/unit1_ast/gate_payload.hpp"
#include "qroute/unit1_ast/lexer.hpp"
#include "qroute/unit1_ast/qasm_parser.hpp"
#include "qroute/unit1_ast/threaded_ast.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace qroute;
using namespace qroute::unit1;

// T1.1: Size and alignment contracts
TEST(Unit1AST, T1_1_SizeAndAlignmentContracts) {
    ASSERT_EQ(sizeof(ASTNode), 24ULL);
    ASSERT_EQ(alignof(ASTNode), 4ULL);
    ASSERT_EQ(sizeof(GatePayload), 64ULL);
}

// T1.2: Lexer tokenization tests (L-1 through L-7)
TEST(Unit1AST, T1_2_LexerTokenization) {
    std::string src =
        "// OpenQASM comment\n"
        "# Another comment\n"
        "OPENQASM 2.0;\n"
        "include \"qelib1.inc\";\n"
        "qreg q[5];\n"
        "h q[0];\n"
        "rx(0.45) q[1];\n"
        "ry(.5) q[2];\n"
        "rz(1e-3) q[3];\n"
        "rz(pi/4) q[4];\n"
        "cx q[0], q[1];\n"
        "measure q[0] -> c[0];\n";

    Lexer lexer(src);
    std::vector<Token> tokens = lexer.tokenize();

    ASSERT_FALSE(tokens.empty());
    ASSERT_EQ(tokens[0].type, TokenType::OpenQasm);
    ASSERT_EQ(tokens[0].line, 3ULL); // lines 1 and 2 are comments

    // Check pi token
    bool found_pi = false;
    for (const auto& tok : tokens) {
        if (tok.type == TokenType::Pi) {
            found_pi = true;
            ASSERT_NEAR(tok.numeric, 3.14159265358979323846, 1e-12);
        }
        if (tok.type == TokenType::Arrow) {
            ASSERT_EQ(tok.text, "->");
        }
        if (tok.type == TokenType::String) {
            ASSERT_EQ(tok.text, "qelib1.inc");
        }
    }
    ASSERT_TRUE(found_pi);

    // L-7: Unknown character throws
    Lexer bad_lexer("qreg q[1]; @ bad character;");
    ASSERT_THROW(bad_lexer.tokenize(), std::invalid_argument);
}

// T1.3: Parser on data/circuit.qasm
TEST(Unit1AST, T1_3_ParserCircuitQasm) {
    ParsedProgram prog = QasmParser::parse_file("data/circuit.qasm");

    // Derived counting contracts
    ASSERT_EQ(prog.ast.size(), 18ULL);
    ASSERT_EQ(prog.registers.size(), 2ULL);
    ASSERT_EQ(prog.total_logical_qubits, 5ULL);
    ASSERT_EQ(prog.total_classical_bits, 5ULL);

    // Validate registers
    const auto* qreg = prog.registers.find("q");
    ASSERT_TRUE(qreg != nullptr);
    ASSERT_TRUE(qreg->is_quantum);
    ASSERT_EQ(qreg->width, 5ULL);
    ASSERT_EQ(qreg->base_index, 0ULL);

    const auto* creg = prog.registers.find("c");
    ASSERT_TRUE(creg != nullptr);
    ASSERT_FALSE(creg->is_quantum);
    ASSERT_EQ(creg->width, 5ULL);
    ASSERT_EQ(creg->base_index, 0ULL);

    // Verify traversal order equals ascending seq == source order
    uint32_t expected_seq = 0;
    prog.ast.traverse([&](NodeIndex idx, const ASTNode& n, const GatePayload& /*p*/) {
        ASSERT_EQ(n.seq, expected_seq);
        ASSERT_EQ(idx, expected_seq);
        ++expected_seq;
    });
    ASSERT_EQ(expected_seq, 18ULL);
}

// T1.4: ThreadedAST validate_threads on empty / 1-node / 1000-node trees
TEST(Unit1AST, T1_4_ValidateThreads) {
    // Empty tree
    ThreadedAST empty_tree;
    empty_tree.build_from_sequence();
    empty_tree.finalize_threads();
    ASSERT_TRUE(empty_tree.validate_threads());

    // 1-node tree
    ThreadedAST single_tree;
    GatePayload gp{};
    single_tree.append(OpCode::H, gp, 1);
    single_tree.build_from_sequence();
    single_tree.finalize_threads();
    ASSERT_TRUE(single_tree.validate_threads());
    ASSERT_EQ(single_tree.leftmost(single_tree.root()), 0ULL);
    ASSERT_EQ(single_tree.next(0), kNil);

    // 1000-node tree
    ThreadedAST large_tree;
    large_tree.reserve(1000);
    for (uint32_t i = 0; i < 1000; ++i) {
        large_tree.append(OpCode::H, gp, i + 1);
    }
    large_tree.build_from_sequence();
    large_tree.finalize_threads();
    ASSERT_TRUE(large_tree.validate_threads());
    ASSERT_TRUE(large_tree.is_balanced());
}

// T1.5: Traversal uses zero auxiliary stack containers and matches oracle
TEST(Unit1AST, T1_5_TraversalMatchesOracle) {
    ThreadedAST tree;
    GatePayload gp{};
    const size_t n = 250;
    for (size_t i = 0; i < n; ++i) {
        tree.append(OpCode::X, gp, static_cast<uint32_t>(i + 1));
    }
    tree.build_from_sequence();
    tree.finalize_threads();

    auto oracle_order = tree.inorder_reference();
    std::vector<NodeIndex> threaded_order;
    threaded_order.reserve(n);
    for (NodeIndex cur = tree.leftmost(tree.root()); cur != kNil; cur = tree.next(cur)) {
        threaded_order.push_back(cur);
    }

    ASSERT_EQ(oracle_order.size(), n);
    ASSERT_EQ(threaded_order.size(), n);
    for (size_t i = 0; i < n; ++i) {
        ASSERT_EQ(threaded_order[i], oracle_order[i]);
        ASSERT_EQ(threaded_order[i], static_cast<NodeIndex>(i));
    }
}

// T1.6: ThreadedAST prev() backwards traversal
TEST(Unit1AST, T1_6_BackwardsTraversalPrev) {
    ThreadedAST tree;
    GatePayload gp{};
    const size_t n = 50;
    for (size_t i = 0; i < n; ++i) {
        tree.append(OpCode::Z, gp, static_cast<uint32_t>(i + 1));
    }
    tree.build_from_sequence();
    tree.finalize_threads();

    // Find the rightmost node
    NodeIndex cur = tree.root();
    while (cur != kNil && tree.node(cur).right != kNil && tree.node(cur).right_thread == 0) {
        cur = tree.node(cur).right;
    }

    std::vector<NodeIndex> backwards;
    while (cur != kNil) {
        backwards.push_back(cur);
        cur = tree.prev(cur);
    }

    ASSERT_EQ(backwards.size(), n);
    for (size_t i = 0; i < n; ++i) {
        ASSERT_EQ(backwards[i], static_cast<NodeIndex>(n - 1 - i));
    }
}

// T1.7: AVL 10k sorted inserts (worst-case)
TEST(Unit1AST, T1_7_AvlWorstCase10kInserts) {
    AvlSymbolTable<int> avl;
    constexpr int kCount = 10000;

    for (int i = 0; i < kCount; ++i) {
        std::ostringstream ss;
        ss << "key_" << std::setw(5) << std::setfill('0') << i;
        bool inserted = avl.insert(ss.str(), i);
        ASSERT_TRUE(inserted);
    }

    ASSERT_EQ(avl.size(), static_cast<size_t>(kCount));
    ASSERT_TRUE(avl.is_balanced());

    // Theoretical bound: 1.44 * log2(10000) + 1 <= 20.14
    uint32_t h = avl.height();
    ASSERT_LE(h, 21ULL);

    // Duplicate insert returns false
    ASSERT_FALSE(avl.insert("key_00000", 999));
    ASSERT_EQ(avl.size(), static_cast<size_t>(kCount));

    // Lookup test
    const int* val = avl.find("key_05432");
    ASSERT_TRUE(val != nullptr);
    ASSERT_EQ(*val, 5432);

    // Inorder verification
    auto in = avl.inorder();
    ASSERT_EQ(in.size(), static_cast<size_t>(kCount));
    for (size_t i = 1; i < in.size(); ++i) {
        ASSERT_TRUE(in[i - 1].key < in[i].key);
    }
}

// T1.8: AVL all four rotation cases (LL, RR, LR, RL)
TEST(Unit1AST, T1_8_AvlRotations) {
    // LL rotation: insert 3, 2, 1
    {
        AvlSymbolTable<int> t;
        t.insert("3", 3);
        t.insert("2", 2);
        t.insert("1", 1);
        ASSERT_TRUE(t.is_balanced());
        ASSERT_EQ(t.height(), 2ULL);
    }

    // RR rotation: insert 1, 2, 3
    {
        AvlSymbolTable<int> t;
        t.insert("1", 1);
        t.insert("2", 2);
        t.insert("3", 3);
        ASSERT_TRUE(t.is_balanced());
        ASSERT_EQ(t.height(), 2ULL);
    }

    // LR rotation: insert 3, 1, 2
    {
        AvlSymbolTable<int> t;
        t.insert("3", 3);
        t.insert("1", 1);
        t.insert("2", 2);
        ASSERT_TRUE(t.is_balanced());
        ASSERT_EQ(t.height(), 2ULL);
    }

    // RL rotation: insert 1, 3, 2
    {
        AvlSymbolTable<int> t;
        t.insert("1", 1);
        t.insert("3", 3);
        t.insert("2", 2);
        ASSERT_TRUE(t.is_balanced());
        ASSERT_EQ(t.height(), 2ULL);
    }
}

// T1.9: Parser error handling rules P-1 to P-8
TEST(Unit1AST, T1_9_ParserErrorHandling) {
    QasmParser parser;

    // P-1: Missing OPENQASM 2.0
    ASSERT_THROW(parser.parse("qreg q[2];"), std::invalid_argument);

    // P-3: Undeclared register
    ASSERT_THROW(parser.parse("OPENQASM 2.0; h q[0];"), std::invalid_argument);

    // P-4: Duplicate register
    ASSERT_THROW(parser.parse("OPENQASM 2.0; qreg q[2]; qreg q[3];"), std::invalid_argument);

    // P-5: Qubit index out of range
    ASSERT_THROW(parser.parse("OPENQASM 2.0; qreg q[2]; h q[2];"), std::invalid_argument);

    // P-6: Identical operands in CX
    ASSERT_THROW(parser.parse("OPENQASM 2.0; qreg q[2]; cx q[1], q[1];"), std::invalid_argument);

    // P-7: Arity mismatch
    ASSERT_THROW(parser.parse("OPENQASM 2.0; qreg q[2]; rx q[0];"), std::invalid_argument);

    // P-8: Unsupported gate
    ASSERT_THROW(parser.parse("OPENQASM 2.0; qreg q[2]; toffoli q[0], q[1];"), std::invalid_argument);
}

// T1.10: Constant folding for gate angles
TEST(Unit1AST, T1_10_GateAngleConstantFolding) {
    std::string src =
        "OPENQASM 2.0;\n"
        "qreg q[4];\n"
        "rx(pi/4) q[0];\n"
        "ry((0.1 + 0.2) * 2) q[1];\n"
        "rz(-pi / 2) q[2];\n"
        "rx(1.5 + 2.5 * 3 - 4 / 2) q[3];\n";

    ParsedProgram prog = QasmParser().parse(src);
    ASSERT_EQ(prog.ast.size(), 6ULL); // header + qreg + 4 gates

    // rx(pi/4) is node index 2
    ASSERT_NEAR(prog.ast.payload(prog.ast.node(2).payload).params[0],
                3.14159265358979323846 / 4.0, 1e-9);

    // ry((0.1 + 0.2) * 2) is node index 3 -> 0.6
    ASSERT_NEAR(prog.ast.payload(prog.ast.node(3).payload).params[0],
                0.6, 1e-9);

    // rz(-pi / 2) is node index 4
    ASSERT_NEAR(prog.ast.payload(prog.ast.node(4).payload).params[0],
                -3.14159265358979323846 / 2.0, 1e-9);

    // rx(1.5 + 7.5 - 2 = 7.0) is node index 5
    ASSERT_NEAR(prog.ast.payload(prog.ast.node(5).payload).params[0],
                7.0, 1e-9);
}

// T1.11: Multi-register flattened indexing
TEST(Unit1AST, T1_11_MultiRegisterFlattenedIndexing) {
    std::string src =
        "OPENQASM 2.0;\n"
        "qreg a[2];\n"
        "qreg b[3];\n"
        "creg c[2];\n"
        "creg d[3];\n"
        "h a[1];\n"
        "cx a[1], b[2];\n"
        "measure b[2] -> d[1];\n";

    ParsedProgram prog = QasmParser().parse(src);
    ASSERT_EQ(prog.total_logical_qubits, 5ULL);
    ASSERT_EQ(prog.total_classical_bits, 5ULL);

    // h a[1] -> node 5. a has base 0, so a[1] -> qubit 1
    const auto& h_node = prog.ast.node(5);
    ASSERT_EQ(prog.ast.payload(h_node.payload).qubits[0], 1ULL);

    // cx a[1], b[2] -> node 6. b has base 2, so b[2] -> qubit 2 + 2 = 4
    const auto& cx_node = prog.ast.node(6);
    ASSERT_EQ(prog.ast.payload(cx_node.payload).qubits[0], 1ULL);
    ASSERT_EQ(prog.ast.payload(cx_node.payload).qubits[1], 4ULL);

    // measure b[2] -> d[1] -> node 7. d has base 2, so d[1] -> clbit 2 + 1 = 3
    const auto& m_node = prog.ast.node(7);
    ASSERT_EQ(prog.ast.payload(m_node.payload).qubits[0], 4ULL);
    ASSERT_EQ(prog.ast.payload(m_node.payload).clbits[0], 3ULL);
}

int main(int argc, char** argv) {
    return qroute::test::run_all_tests(argc, argv);
}
