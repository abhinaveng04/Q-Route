#include "qroute/pipeline/execution_engine.hpp"

#include "qroute/common/constants.hpp"
#include "qroute/common/deterministic_time.hpp"
#include "qroute/pipeline/report_writer.hpp"
#include "qroute/unit5_heap/telemetry_config.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace qroute::pipeline {

namespace {

std::string format_param(double val) {
    constexpr double kPi = 3.14159265358979323846;
    if (std::abs(val - kPi / 4.0) < 1e-4) {
        return "pi/4";
    }
    if (std::abs(val - kPi / 2.0) < 1e-4) {
        return "pi/2";
    }
    if (std::abs(val - kPi) < 1e-4) {
        return "pi";
    }
    if (std::abs(val - 3.0 * kPi / 4.0) < 1e-4) {
        return "3*pi/4";
    }
    std::ostringstream oss;
    oss << std::defaultfloat << std::setprecision(6) << val;
    return oss.str();
}

} // namespace

std::string RoutedCircuit::to_qasm_text(
    const unit2::PhysicalCouplingGraph& graph,
    const std::string& circuit_file,
    const std::string& coupling_file,
    const Metrics& metrics) const
{
    std::ostringstream oss;
    oss << "// circuit            : " << circuit_file << "\n";
    oss << "// coupling           : " << coupling_file << "\n";
    oss << "// pipeline_revision  : R2\n";
    oss << "// logical_qubits     : " << metrics.logical_qubits << "\n";
    oss << "// physical_qubits    : " << physical_qubits << "\n";
    oss << "// inserted_swaps     : " << metrics.inserted_swap_count << "\n";
    oss << "// added_cnots        : " << metrics.added_cnot_count << "\n";
    oss << "// cumulative_fidelity: " << std::fixed << std::setprecision(6)
        << metrics.cumulative_fidelity << "\n";

    oss << "OPENQASM 2.0;\n";
    oss << "include \"qelib1.inc\";\n";
    oss << "qreg p[" << physical_qubits << "];\n";
    oss << "creg c[" << classical_bits << "];\n";

    for (const auto& g : gates) {
        if (g.op == OpCode::CX) {
            assert(g.qubits.size() == 2);
            PhysicalQubit c = g.qubits[0];
            PhysicalQubit t = g.qubits[1];

            // Mandatory hard adjacency assertion at write time
            if (!graph.has_edge(c, t)) {
                throw std::logic_error("Physical adjacency violation for CX between p[" +
                                       std::to_string(c) + "] and p[" + std::to_string(t) + "]");
            }

            oss << "cx p[" << c << "],p[" << t << "];";
            if (g.is_synthetic_swap) {
                oss << "      // swap-component " << g.swap_step << "/3";
                if (g.swap_step == 1) {
                    oss << " (logical SWAP p" << g.swap_p1 << "p" << g.swap_p2 << ")";
                }
                oss << "\n";
            } else if (g.routed_ctrl != kNil && g.routed_tgt != kNil) {
                oss << "      // routed CX (logical cx q[" << g.routed_ctrl << "],q["
                    << g.routed_tgt << "])\n";
            } else {
                oss << "\n";
            }
        } else if (g.op == OpCode::Barrier) {
            oss << "barrier";
            for (size_t i = 0; i < g.qubits.size(); ++i) {
                oss << (i == 0 ? " " : ",") << "p[" << g.qubits[i] << "]";
            }
            oss << ";\n";
        } else if (g.op == OpCode::Measure) {
            assert(!g.qubits.empty());
            oss << "measure p[" << g.qubits[0] << "] -> c[" << g.creg_idx << "];\n";
        } else {
            // Single-qubit unitary
            std::string op_name;
            switch (g.op) {
                case OpCode::H:  op_name = "h"; break;
                case OpCode::X:  op_name = "x"; break;
                case OpCode::Y:  op_name = "y"; break;
                case OpCode::Z:  op_name = "z"; break;
                case OpCode::RX: op_name = "rx"; break;
                case OpCode::RY: op_name = "ry"; break;
                case OpCode::RZ: op_name = "rz"; break;
                default: op_name = "u"; break;
            }

            oss << op_name;
            if (!g.params.empty()) {
                oss << "(";
                for (size_t p = 0; p < g.params.size(); ++p) {
                    if (p > 0) oss << ",";
                    oss << format_param(g.params[p]);
                }
                oss << ")";
            }
            oss << " p[" << g.qubits[0] << "];\n";
        }
    }

    return oss.str();
}

ExecutionEngine::ExecutionEngine(EngineConfig cfg)
    : cfg_(std::move(cfg))
{}

int ExecutionEngine::run() {
    auto total_t0 = std::chrono::steady_clock::now();

    try {
        // Phase 1: AST Ingestion [Unit 1]
        try {
            phase1_parse();
        } catch (const std::exception& e) {
            std::string msg = e.what();
            if (msg.find("open file") != std::string::npos ||
                msg.find("Cannot open") != std::string::npos ||
                msg.find("Failed to open") != std::string::npos) {
                std::cerr << "File Error: " << msg << "\n";
                return 2;
            }
            std::cerr << "OpenQASM Parse Error: " << msg << "\n";
            return 3;
        }

        // Phase 4: Placement via DFS B&B [Unit 4]
        try {
            phase4_place();
        } catch (const std::exception& e) {
            std::string msg = e.what();
            if (msg.find("open file") != std::string::npos ||
                msg.find("Cannot open") != std::string::npos ||
                msg.find("Failed to open") != std::string::npos) {
                std::cerr << "File Error: " << msg << "\n";
                return 2;
            }
            if (msg.find("Placement infeasible") != std::string::npos) {
                std::cerr << "Placement Infeasible: " << msg << "\n";
                return 5;
            }
            if (msg.find("Coupling") != std::string::npos ||
                msg.find("coupling") != std::string::npos ||
                msg.find("fidelity") != std::string::npos ||
                msg.find("vertex") != std::string::npos ||
                msg.find("edge") != std::string::npos ||
                msg.find("self-loop") != std::string::npos ||
                msg.find("duplicate") != std::string::npos ||
                msg.find("header") != std::string::npos) {
                std::cerr << "Coupling Map Error: " << msg << "\n";
                return 4;
            }
            std::cerr << "Placement Error: " << msg << "\n";
            return 5;
        }

        // Phase 2: Routing via Dijkstra [Unit 2]
        try {
            phase2_route();
        } catch (const unit2::RoutingError& e) {
            std::cerr << "Routing Error: " << e.what() << "\n";
            return 6;
        }

        // Phase 3: Contraction Planning [Unit 3]
        phase3_plan();

        // Phase 5: Telemetry Broker [Unit 5]
        phase5_telemetry();

        // Depth and Fidelity Post-Processing
        compute_depth_metrics();
        compute_fidelity_metrics();

        auto total_t1 = std::chrono::steady_clock::now();
        metrics_.total_cpu_time_us = std::chrono::duration<double, std::micro>(total_t1 - total_t0).count();

        // Final Output Generation
        emit_outputs();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Execution Error: " << e.what() << "\n";
        return 2;
    }
}

void ExecutionEngine::phase1_parse() {
    auto t0 = std::chrono::steady_clock::now();
    program_ = unit1::QasmParser::parse_file(cfg_.circuit_path);
    auto t1 = std::chrono::steady_clock::now();

    metrics_.parse_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    metrics_.ast_build_time_us = metrics_.parse_time_us; // parsing builds AST
    metrics_.ast_nodes = program_.ast.size();
    metrics_.symbol_table_entries = program_.registers.size();
    metrics_.thread_validation = program_.ast.validate_threads();

    metrics_.logical_qubits = program_.total_logical_qubits;
    routed_.classical_bits = program_.total_classical_bits;

    // Inspect instructions in AST
    program_.ast.traverse([&](NodeIndex /*idx*/, const unit1::ASTNode& node, const unit1::GatePayload& payload) {
        if (node.op == OpCode::CX) {
            ++metrics_.initial_two_qubit_count;
            ++metrics_.initial_cnot_count;
            ++metrics_.initial_gate_count;
        } else if (is_two_qubit_gate(node.op)) {
            ++metrics_.initial_two_qubit_count;
            ++metrics_.initial_gate_count;
        } else if (is_single_qubit_gate(node.op)) {
            ++metrics_.initial_single_qubit_count;
            ++metrics_.initial_gate_count;
        } else if (node.op == OpCode::Measure) {
            ++metrics_.initial_measure_count;
        } else if (node.op == OpCode::Barrier) {
            ++metrics_.initial_barrier_count;
        }

        // Track max logical qubit touched
        for (uint8_t k = 0; k < payload.qubit_count; ++k) {
            LogicalQubit q = payload.qubits[k];
            if (q + 1 > metrics_.logical_qubits) {
                metrics_.logical_qubits = q + 1;
            }
        }
    });

    if (routed_.classical_bits == 0) {
        routed_.classical_bits = metrics_.logical_qubits;
    }
}

void ExecutionEngine::phase4_place() {
    auto t0 = std::chrono::steady_clock::now();
    graph_ = std::make_unique<unit2::PhysicalCouplingGraph>(
        unit2::PhysicalCouplingGraph::load_from_file(cfg_.coupling_path));
    metrics_.physical_qubits = graph_->vertex_count();
    routed_.physical_qubits = metrics_.physical_qubits;

    lookahead_ = unit4::LookaheadGraph(metrics_.logical_qubits);
    lookahead_.build(program_.ast, cfg_.lookahead_window);
    if (lookahead_.logical_count() > metrics_.logical_qubits) {
        metrics_.logical_qubits = lookahead_.logical_count();
    }
    metrics_.lookahead_edges = lookahead_.edges().size();

    // Check feasibility constraint: n <= m
    if (metrics_.logical_qubits > metrics_.physical_qubits) {
        metrics_.search_completed = false;
        throw std::runtime_error("Placement infeasible: logical qubit count (" +
                                 std::to_string(metrics_.logical_qubits) +
                                 ") exceeds physical qubit count (" +
                                 std::to_string(metrics_.physical_qubits) + ")");
    }

    unit4::PlacementConfig pcfg;
    pcfg.logical_count = metrics_.logical_qubits;
    pcfg.physical_count = metrics_.physical_qubits;
    pcfg.graph = graph_.get();
    pcfg.lookahead = lookahead_;
    pcfg.time_limit_ms = cfg_.placement_time_limit_ms;
    pcfg.node_budget = cfg_.node_budget;

    unit4::QubitPlacementBAndB solver(pcfg);
    placement_ = solver.solve();

    auto t1 = std::chrono::steady_clock::now();
    metrics_.placement_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    metrics_.greedy_upper_bound_cost = placement_.greedy_upper_bound_cost;
    metrics_.placement_cost_hops = placement_.best_cost;
    metrics_.nodes_expanded = placement_.nodes_expanded;
    metrics_.branches_pruned = placement_.branches_pruned;
    metrics_.max_stack_depth = placement_.max_stack_depth;
    metrics_.search_completed = placement_.completed;

    if (!placement_.feasible) {
        throw std::runtime_error("Placement infeasible: disconnected required pairs");
    }

    // Initialize placement map
    map_ = unit2::PlacementMap(metrics_.logical_qubits, metrics_.physical_qubits);
    map_.load(placement_.logical_to_physical);
}

void ExecutionEngine::phase2_route() {
    auto t0 = std::chrono::steady_clock::now();

    unit2::SwapSynthesizer synth;
    routed_.gates.clear();
    uint32_t total_swaps = 0;
    uint32_t dijkstra_invocations = 0;

    program_.ast.traverse([&](NodeIndex /*idx*/, const unit1::ASTNode& node, const unit1::GatePayload& payload) {
        if (node.op == OpCode::CX) {
            LogicalQubit la = payload.qubits[0];
            LogicalQubit lb = payload.qubits[1];

            PhysicalQubit pa = map_.physical_of(la);
            PhysicalQubit pb = map_.physical_of(lb);

            if (!graph_->has_edge(pa, pb)) {
                ++dijkstra_invocations;
            }

            auto plan = synth.route(*graph_, map_, la, lb);
            total_swaps += plan.swap_count;

            uint32_t current_swap_step = 0;
            PhysicalQubit swap_u = kNil;
            PhysicalQubit swap_v = kNil;

            for (const auto& g : plan.gates) {
                EmittedPhysicalGate pg;
                pg.op = OpCode::CX;
                pg.qubits = {g.control, g.target};
                pg.duration_ns = g.duration_ns;
                pg.fidelity = g.fidelity;
                pg.weight = g.weight;

                if (g.is_synthetic_swap_component) {
                    pg.is_synthetic_swap = true;
                    current_swap_step = (current_swap_step % 3) + 1;
                    pg.swap_step = current_swap_step;
                    if (current_swap_step == 1) {
                        swap_u = std::min(g.control, g.target);
                        swap_v = std::max(g.control, g.target);
                    }
                    pg.swap_p1 = swap_u;
                    pg.swap_p2 = swap_v;
                } else {
                    current_swap_step = 0;
                    pg.is_synthetic_swap = false;
                    pg.routed_ctrl = la;
                    pg.routed_tgt = lb;
                }
                routed_.gates.push_back(pg);
            }
        } else if (is_single_qubit_gate(node.op)) {
            LogicalQubit l = payload.qubits[0];
            PhysicalQubit p = map_.physical_of(l);

            EmittedPhysicalGate pg;
            pg.op = node.op;
            pg.qubits = {p};
            for (uint8_t i = 0; i < payload.param_count; ++i) {
                pg.params.push_back(payload.params[i]);
            }
            pg.duration_ns = kSingleQubitNs;
            pg.fidelity = 1.0;
            pg.weight = 0.0;
            routed_.gates.push_back(pg);
        } else if (node.op == OpCode::Measure) {
            LogicalQubit l = payload.qubits[0];
            PhysicalQubit p = map_.physical_of(l);

            EmittedPhysicalGate pg;
            pg.op = OpCode::Measure;
            pg.qubits = {p};
            pg.creg_idx = l; // measure to matching classical bit
            pg.duration_ns = kMeasureNs;
            pg.fidelity = 1.0;
            pg.weight = 0.0;
            routed_.gates.push_back(pg);
        } else if (node.op == OpCode::Barrier) {
            EmittedPhysicalGate pg;
            pg.op = OpCode::Barrier;
            for (uint8_t i = 0; i < payload.qubit_count; ++i) {
                pg.qubits.push_back(map_.physical_of(payload.qubits[i]));
            }
            pg.duration_ns = 0.0;
            pg.fidelity = 1.0;
            pg.weight = 0.0;
            routed_.gates.push_back(pg);
        }
    });

    auto t1 = std::chrono::steady_clock::now();
    metrics_.routing_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    metrics_.inserted_swap_count = total_swaps;
    metrics_.added_cnot_count = 3 * total_swaps;
    metrics_.final_cnot_count = metrics_.initial_cnot_count + metrics_.added_cnot_count;
    metrics_.final_physical_gate_count = routed_.gates.size();
    metrics_.dijkstra_invocations = dijkstra_invocations;
}

void ExecutionEngine::phase3_plan() {
    auto t0 = std::chrono::steady_clock::now();

    std::vector<uint64_t> dims;
    if (!cfg_.chain_path.empty()) {
        dims = unit3::MatrixChainOptimizer::load_chain_file(cfg_.chain_path);
    } else {
        dims = unit3::MatrixChainOptimizer::derive_chain_from_ast(program_.ast);
    }

    unit3::MatrixChainOptimizer opt(dims);
    plan_ = opt.build_plan();

    auto t1 = std::chrono::steady_clock::now();
    metrics_.mcm_plan_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    metrics_.chain_length = plan_.chain_length;
    metrics_.mcm_optimal_cost = plan_.optimal_cost;
    metrics_.mcm_left_to_right_cost = plan_.left_to_right_cost;
    metrics_.mcm_savings = plan_.savings;
    metrics_.mcm_savings_percent = plan_.savings_percent;
    metrics_.mcm_overflow_detected = plan_.overflow_detected;
    metrics_.mcm_parenthesization = plan_.parenthesization;
}

void ExecutionEngine::phase5_telemetry() {
    auto t0 = std::chrono::steady_clock::now();

    unit5::TelemetryConfig tcfg;
    if (!cfg_.telemetry_path.empty()) {
        tcfg = unit5::TelemetryConfig::load_from_file(cfg_.telemetry_path);
    } else {
        // Try data/telemetry.conf if exists, else defaults
        try {
            tcfg = unit5::TelemetryConfig::load_from_file("data/telemetry.conf");
        } catch (...) {
            // Keep defaults
        }
    }

    broker_ = std::make_unique<unit5::TelemetryBroker>(metrics_.physical_qubits, tcfg);

    uint64_t gate_idx = 0;
    for (const auto& g : routed_.gates) {
        ++gate_idx;
        double dur = g.duration_ns;
        for (PhysicalQubit q : g.qubits) {
            broker_->on_gate_executed(q, dur, gate_idx);
        }
    }

    alerts_ = broker_->drain_critical();

    auto t1 = std::chrono::steady_clock::now();
    metrics_.telemetry_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    auto stats = broker_->heap_stats();
    metrics_.telemetry_inserts = stats.inserts;
    metrics_.telemetry_decrease_keys = stats.decrease_keys;
    metrics_.telemetry_extract_mins = stats.extract_mins;
    metrics_.alerts_dispatched = alerts_.size();
}

void ExecutionEngine::compute_depth_metrics() {
    // 1. Pre-routing logical DAG depth (depth_before)
    std::vector<uint64_t> logical_qubit_depth(std::max(metrics_.logical_qubits, 1U), 0);
    program_.ast.traverse([&](NodeIndex /*idx*/, const unit1::ASTNode& node, const unit1::GatePayload& payload) {
        if (!is_single_qubit_gate(node.op) && !is_two_qubit_gate(node.op)) {
            return;
        }
        uint64_t max_d = 0;
        for (uint8_t k = 0; k < payload.qubit_count; ++k) {
            LogicalQubit q = payload.qubits[k];
            if (q < logical_qubit_depth.size()) {
                max_d = std::max(max_d, logical_qubit_depth[q]);
            }
        }
        uint64_t next_d = max_d + 1;
        for (uint8_t k = 0; k < payload.qubit_count; ++k) {
            LogicalQubit q = payload.qubits[k];
            if (q < logical_qubit_depth.size()) {
                logical_qubit_depth[q] = next_d;
            }
        }
    });

    metrics_.depth_before = 0;
    for (uint64_t d : logical_qubit_depth) {
        metrics_.depth_before = std::max(metrics_.depth_before, d);
    }

    // 2. Post-routing physical DAG depth (depth_after)
    std::vector<uint64_t> physical_qubit_depth(std::max(metrics_.physical_qubits, 1U), 0);
    for (const auto& g : routed_.gates) {
        if (g.op == OpCode::Barrier) {
            continue;
        }
        uint64_t max_d = 0;
        for (PhysicalQubit q : g.qubits) {
            if (q < physical_qubit_depth.size()) {
                max_d = std::max(max_d, physical_qubit_depth[q]);
            }
        }
        uint64_t next_d = max_d + 1;
        for (PhysicalQubit q : g.qubits) {
            if (q < physical_qubit_depth.size()) {
                physical_qubit_depth[q] = next_d;
            }
        }
    }

    metrics_.depth_after = 0;
    for (uint64_t d : physical_qubit_depth) {
        metrics_.depth_after = std::max(metrics_.depth_after, d);
    }

    if (metrics_.depth_before > 0) {
        metrics_.depth_expansion_percent = 100.0 *
            static_cast<double>(metrics_.depth_after - metrics_.depth_before) /
            static_cast<double>(metrics_.depth_before);
    } else {
        metrics_.depth_expansion_percent = 0.0;
    }
}

void ExecutionEngine::compute_fidelity_metrics() {
    double total_log_infidelity = 0.0;

    for (const auto& g : routed_.gates) {
        if (g.op == OpCode::CX) {
            assert(g.qubits.size() == 2);
            PhysicalQubit c = g.qubits[0];
            PhysicalQubit t = g.qubits[1];
            double w = graph_->edge_weight(c, t);
            total_log_infidelity += w;
        }
    }

    metrics_.log_infidelity = total_log_infidelity;
    metrics_.cumulative_fidelity = std::exp(-total_log_infidelity);
}

void ExecutionEngine::emit_outputs() {
    // 1. Calculate determinism_hash
    metrics_.determinism_hash = ReportWriter::compute_determinism_hash(
        metrics_, cfg_.circuit_path, cfg_.coupling_path);

    // 2. Write routed QASM output
    if (!cfg_.out_path.empty()) {
        ReportWriter::write_routed_qasm(
            cfg_.out_path, routed_, *graph_, cfg_.circuit_path, cfg_.coupling_path, metrics_);
    }

    // 3. Write JSON report if requested
    if (!cfg_.out_json_path.empty()) {
        ReportWriter::write_json(
            cfg_.out_json_path, metrics_, cfg_.circuit_path, cfg_.coupling_path,
            cfg_.lookahead_window, placement_.logical_to_physical, plan_, alerts_);
    }

    // 4. Write stdout report
    ReportWriter::write_stdout(metrics_, cfg_.circuit_path, cfg_.coupling_path, std::cout);
}

} // namespace qroute::pipeline
