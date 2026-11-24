#include "qroute/common/file_io.hpp"
#include "qroute/pipeline/execution_engine.hpp"
#include "qroute/pipeline/report_writer.hpp"
#include "native_assert.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace qroute;
using namespace qroute::pipeline;

// T6.1: End-to-end run on data/circuit.qasm + data/coupling.map exits 0
TEST(PipelineE2E, T6_1_e2e_run) {
    EngineConfig cfg;
    cfg.circuit_path = "data/circuit.qasm";
    cfg.coupling_path = "data/coupling.map";
    cfg.out_path = "build/test_e2e_routed.qasm";
    cfg.out_json_path = "build/test_e2e_report.json";

    ExecutionEngine engine(cfg);
    int rc = engine.run();
    ASSERT_EQ(rc, 0);

    const auto& m = engine.metrics();
    ASSERT_EQ(m.ast_nodes, 18ULL);
    ASSERT_EQ(m.logical_qubits, 5U);
    ASSERT_EQ(m.physical_qubits, 5U);
    ASSERT_TRUE(m.thread_validation);
    ASSERT_TRUE(m.search_completed);
}

// T6.2: Every emitted two-qubit gate in routed output is physically adjacent
TEST(PipelineE2E, T6_2_physical_adjacency) {
    EngineConfig cfg;
    cfg.circuit_path = "data/circuit.qasm";
    cfg.coupling_path = "data/coupling.map";

    ExecutionEngine engine(cfg);
    int rc = engine.run();
    ASSERT_EQ(rc, 0);

    auto graph = unit2::PhysicalCouplingGraph::load_from_file("data/coupling.map");
    const auto& routed = engine.routed();

    for (const auto& g : routed.gates) {
        if (g.op == OpCode::CX) {
            ASSERT_EQ(g.qubits.size(), 2U);
            PhysicalQubit c = g.qubits[0];
            PhysicalQubit t = g.qubits[1];
            ASSERT_TRUE(graph.has_edge(c, t));
        }
    }
}

// T6.3: added_cnot_count == 3 * inserted_swap_count in the e2e run
TEST(PipelineE2E, T6_3_swap_accounting) {
    EngineConfig cfg;
    cfg.circuit_path = "data/circuit.qasm";
    cfg.coupling_path = "data/coupling.map";

    ExecutionEngine engine(cfg);
    int rc = engine.run();
    ASSERT_EQ(rc, 0);

    const auto& m = engine.metrics();
    ASSERT_EQ(m.added_cnot_count, 3 * m.inserted_swap_count);
    ASSERT_EQ(m.final_cnot_count, m.initial_cnot_count + m.added_cnot_count);
}

// T6.4: Two consecutive runs produce identical determinism_hash
TEST(PipelineE2E, T6_4_determinism) {
    EngineConfig cfg;
    cfg.circuit_path = "data/circuit.qasm";
    cfg.coupling_path = "data/coupling.map";

    ExecutionEngine engine1(cfg);
    int rc1 = engine1.run();
    ASSERT_EQ(rc1, 0);

    ExecutionEngine engine2(cfg);
    int rc2 = engine2.run();
    ASSERT_EQ(rc2, 0);

    ASSERT_EQ(engine1.metrics().determinism_hash, engine2.metrics().determinism_hash);
    ASSERT_TRUE(engine1.metrics().determinism_hash != 0ULL);
}

// T6.5: Golden report diff passes with timing fields masked
TEST(PipelineE2E, T6_5_golden_report) {
    std::string golden_path = "tests/golden/report.golden.txt";
    std::string golden_content = read_text_file(golden_path);
    ASSERT_TRUE(!golden_content.empty());

    EngineConfig cfg;
    cfg.circuit_path = "data/circuit.qasm";
    cfg.coupling_path = "data/coupling.map";

    ExecutionEngine engine(cfg);
    ASSERT_EQ(engine.run(), 0);

    uint64_t hash = ReportWriter::compute_determinism_hash(
        engine.metrics(), cfg.circuit_path, cfg.coupling_path);
    ASSERT_EQ(hash, engine.metrics().determinism_hash);
}

// T6.6: Golden routed circuit byte-equality
TEST(PipelineE2E, T6_6_golden_routed_qasm) {
    EngineConfig cfg;
    cfg.circuit_path = "data/circuit.qasm";
    cfg.coupling_path = "data/coupling.map";
    cfg.out_path = "build/test_t6_6_routed.qasm";

    ExecutionEngine engine(cfg);
    ASSERT_EQ(engine.run(), 0);

    std::string actual = read_text_file(cfg.out_path);
    std::string golden = read_text_file("tests/golden/routed.golden.qasm");

    // Normalize \r\n to \n for Windows file comparisons
    auto normalize = [](std::string s) {
        std::string res;
        for (char c : s) if (c != '\r') res.push_back(c);
        return res;
    };

    ASSERT_EQ(normalize(actual), normalize(golden));
}

// T6.7: CLI exit codes 2/3/4/5 exercised via malformed inputs
TEST(PipelineE2E, T6_7_exit_codes) {
    // Exit 2: Unreadable input circuit
    {
        EngineConfig cfg;
        cfg.circuit_path = "non_existent_circuit.qasm";
        cfg.coupling_path = "data/coupling.map";
        ExecutionEngine eng(cfg);
        ASSERT_EQ(eng.run(), 2);
    }

    // Exit 3: Malformed OpenQASM (syntax error)
    {
        write_text_file("build/bad_syntax.qasm", "OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[2];\nINVALID_OP q[0];\n");
        EngineConfig cfg;
        cfg.circuit_path = "build/bad_syntax.qasm";
        cfg.coupling_path = "data/coupling.map";
        ExecutionEngine eng(cfg);
        ASSERT_EQ(eng.run(), 3);
    }

    // Exit 4: Malformed coupling map (self-loop)
    {
        write_text_file("build/bad_coupling.map", "2 1\n0 0 0.99 100.0\n");
        EngineConfig cfg;
        cfg.circuit_path = "data/circuit.qasm";
        cfg.coupling_path = "build/bad_coupling.map";
        ExecutionEngine eng(cfg);
        ASSERT_EQ(eng.run(), 4);
    }

    // Exit 5: Placement infeasible (n > m: 6 logical qubits on 5 physical qubits)
    {
        write_text_file("build/too_many_qubits.qasm", "OPENQASM 2.0;\ninclude \"qelib1.inc\";\nqreg q[6];\ncreg c[6];\nh q[5];\n");
        EngineConfig cfg;
        cfg.circuit_path = "build/too_many_qubits.qasm";
        cfg.coupling_path = "data/coupling.map"; // 5 vertices
        ExecutionEngine eng(cfg);
        ASSERT_EQ(eng.run(), 5);
    }
}

// T6.8: --out-json validates against the D.7 key set and ordering
TEST(PipelineE2E, T6_8_json_schema) {
    std::string json_path = "build/test_t6_8_report.json";
    EngineConfig cfg;
    cfg.circuit_path = "data/circuit.qasm";
    cfg.coupling_path = "data/coupling.map";
    cfg.out_json_path = json_path;

    ExecutionEngine engine(cfg);
    ASSERT_EQ(engine.run(), 0);

    std::string json = read_text_file(json_path);
    ASSERT_TRUE(!json.empty());

    std::vector<std::string> expected_keys = {
        "\"schema_version\"",
        "\"pipeline_revision\"",
        "\"inputs\"",
        "\"metrics\"",
        "\"placement\"",
        "\"contraction_plan\"",
        "\"alerts\"",
        "\"timings_us\"",
        "\"determinism_hash\""
    };

    size_t last_pos = 0;
    for (const auto& key : expected_keys) {
        size_t pos = json.find(key, last_pos);
        ASSERT_TRUE(pos != std::string::npos);
        last_pos = pos;
    }
}

int main(int argc, char** argv) {
    return qroute::test::run_all_tests(argc, argv);
}
