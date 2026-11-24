#pragma once

#include "qroute/common/types.hpp"
#include "qroute/pipeline/metrics.hpp"
#include "qroute/unit1_ast/qasm_parser.hpp"
#include "qroute/unit2_graph/physical_coupling_graph.hpp"
#include "qroute/unit2_graph/placement_map.hpp"
#include "qroute/unit2_graph/swap_synthesizer.hpp"
#include "qroute/unit3_dp/matrix_chain_optimizer.hpp"
#include "qroute/unit4_bnb/lookahead_graph.hpp"
#include "qroute/unit4_bnb/qubit_placement_bnb.hpp"
#include "qroute/unit5_heap/telemetry_broker.hpp"

#include <memory>
#include <string>
#include <vector>

namespace qroute::pipeline {

struct EngineConfig {
    std::string circuit_path;
    std::string coupling_path;
    std::string out_path;
    std::string out_json_path;           // optional
    std::string chain_path;              // optional; overrides derived chain
    std::string telemetry_path;          // optional; defaults to built-in constants
    uint32_t    lookahead_window = 0;    // 0 == full trace
    double      placement_time_limit_ms = 1000.0;
    uint64_t    node_budget = 0;         // 0 == unlimited
    bool        run_benchmarks = false;
    bool        verbose = false;
};

struct EmittedPhysicalGate {
    OpCode op = OpCode::Nop;
    std::vector<PhysicalQubit> qubits;
    std::vector<double> params;
    uint32_t creg_idx = 0;
    bool is_synthetic_swap = false;
    uint32_t swap_step = 0; // 1, 2, or 3
    PhysicalQubit swap_p1 = kNil;
    PhysicalQubit swap_p2 = kNil;
    LogicalQubit routed_ctrl = kNil;
    LogicalQubit routed_tgt = kNil;
    double duration_ns = 0.0;
    double fidelity = 1.0;
    double weight = 0.0;
};

struct RoutedCircuit {
    uint32_t physical_qubits = 0;
    uint32_t classical_bits = 0;
    std::vector<EmittedPhysicalGate> gates;

    std::string to_qasm_text(const unit2::PhysicalCouplingGraph& graph,
                             const std::string& circuit_file,
                             const std::string& coupling_file,
                             const Metrics& metrics) const;
};

class ExecutionEngine {
public:
    explicit ExecutionEngine(EngineConfig cfg);
    int run(); // returns process exit code (Section D.2)

    const Metrics&                metrics() const noexcept { return metrics_; }
    const RoutedCircuit&          routed() const noexcept { return routed_; }
    const unit3::ContractionPlan& plan() const noexcept { return plan_; }
    const std::vector<unit5::Alert>& alerts() const noexcept { return alerts_; }
    const unit4::PlacementResult& placement() const noexcept { return placement_; }

private:
    EngineConfig                                  cfg_;
    Metrics                                       metrics_;
    RoutedCircuit                                 routed_;
    unit3::ContractionPlan                        plan_;
    std::vector<unit5::Alert>                     alerts_;

    unit1::ParsedProgram                          program_;
    std::unique_ptr<unit2::PhysicalCouplingGraph> graph_;
    unit4::LookaheadGraph                         lookahead_;
    unit4::PlacementResult                        placement_;
    unit2::PlacementMap                           map_{0, 0};
    std::unique_ptr<unit5::TelemetryBroker>       broker_;

    void phase1_parse();
    void phase4_place();
    void phase2_route();
    void phase3_plan();
    void phase5_telemetry();
    void compute_depth_metrics();
    void compute_fidelity_metrics();
    void emit_outputs();
};

} // namespace qroute::pipeline
