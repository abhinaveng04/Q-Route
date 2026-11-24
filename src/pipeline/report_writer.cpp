#include "qroute/pipeline/report_writer.hpp"

#include "qroute/common/file_io.hpp"
#include "qroute/common/safe_arithmetic.hpp"
#include "qroute/pipeline/execution_engine.hpp"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace qroute::pipeline {

namespace {

std::string format_fixed(double val, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << val;
    return oss.str();
}

std::string format_hex64(uint64_t val) {
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << val;
    return oss.str();
}

void print_field(std::ostream& out, const std::string& label, const std::string& val) {
    out << std::left << std::setw(26) << label << " : " << val << "\n";
}

} // namespace

uint64_t ReportWriter::compute_determinism_hash(
    const Metrics& m,
    const std::string& circuit_file,
    const std::string& coupling_file)
{
    uint64_t h = SafeArithmetic::kFnv1aOffsetBasis;

    auto feed = [&](const std::string& key, const std::string& val) {
        h = SafeArithmetic::fnv1a_64_str(key, h);
        h = SafeArithmetic::fnv1a_64_str("=", h);
        h = SafeArithmetic::fnv1a_64_str(val, h);
        h = SafeArithmetic::fnv1a_64_str(";", h);
    };

    feed("pipeline_revision", "R2");
    feed("circuit_file", circuit_file);
    feed("coupling_file", coupling_file);

    feed("ast_nodes", std::to_string(m.ast_nodes));
    feed("symbol_table_entries", std::to_string(m.symbol_table_entries));
    feed("thread_validation", m.thread_validation ? "PASS" : "FAIL");

    feed("logical_qubits", std::to_string(m.logical_qubits));
    feed("physical_qubits", std::to_string(m.physical_qubits));
    feed("lookahead_edges", std::to_string(m.lookahead_edges));
    feed("greedy_upper_bound_cost", format_fixed(m.greedy_upper_bound_cost, 3));
    feed("placement_cost_hops", format_fixed(m.placement_cost_hops, 3));
    feed("nodes_expanded", std::to_string(m.nodes_expanded));
    feed("branches_pruned", std::to_string(m.branches_pruned));
    feed("max_stack_depth", std::to_string(m.max_stack_depth));
    feed("search_completed", m.search_completed ? "true" : "false");

    feed("initial_gate_count", std::to_string(m.initial_gate_count));
    feed("initial_single_qubit", std::to_string(m.initial_single_qubit_count));
    feed("initial_two_qubit", std::to_string(m.initial_two_qubit_count));
    feed("initial_cnot_count", std::to_string(m.initial_cnot_count));
    feed("initial_measure_count", std::to_string(m.initial_measure_count));
    feed("initial_barrier_count", std::to_string(m.initial_barrier_count));
    feed("dijkstra_invocations", std::to_string(m.dijkstra_invocations));
    feed("inserted_swap_count", std::to_string(m.inserted_swap_count));
    feed("added_cnot_count", std::to_string(m.added_cnot_count));
    feed("final_cnot_count", std::to_string(m.final_cnot_count));
    feed("final_physical_gates", std::to_string(m.final_physical_gate_count));
    feed("depth_before", std::to_string(m.depth_before));
    feed("depth_after", std::to_string(m.depth_after));
    feed("depth_expansion_percent", format_fixed(m.depth_expansion_percent, 2));
    feed("cumulative_fidelity", format_fixed(m.cumulative_fidelity, 6));
    feed("log_infidelity", format_fixed(m.log_infidelity, 6));

    feed("chain_length", std::to_string(m.chain_length));
    feed("mcm_optimal_cost", m.mcm_overflow_detected ? "SATURATED" : std::to_string(m.mcm_optimal_cost));
    feed("mcm_left_to_right_cost", m.mcm_overflow_detected ? "SATURATED" : std::to_string(m.mcm_left_to_right_cost));
    feed("mcm_savings", m.mcm_overflow_detected ? "SATURATED" : std::to_string(m.mcm_savings));
    feed("mcm_savings_percent", format_fixed(m.mcm_savings_percent, 6));
    feed("mcm_overflow_detected", m.mcm_overflow_detected ? "true" : "false");
    feed("mcm_parenthesization", m.mcm_parenthesization);

    feed("telemetry_inserts", std::to_string(m.telemetry_inserts));
    feed("telemetry_decrease_keys", std::to_string(m.telemetry_decrease_keys));
    feed("telemetry_extract_mins", std::to_string(m.telemetry_extract_mins));
    feed("alerts_dispatched", std::to_string(m.alerts_dispatched));

    return h;
}

void ReportWriter::write_stdout(
    const Metrics& m,
    const std::string& circuit_file,
    const std::string& coupling_file,
    std::ostream& out)
{
    out << "================================================================================\n";
    out << "Q-Route Execution Report\n";
    out << "================================================================================\n";
    print_field(out, "pipeline_revision", "R2");
    print_field(out, "circuit_file", circuit_file);
    print_field(out, "coupling_file", coupling_file);
    out << "\n";

    out << "[Phase 1] Unit 1 - Threaded AST Ingestion\n";
    print_field(out, "parse_time_us", format_fixed(m.parse_time_us, 3));
    print_field(out, "ast_build_time_us", format_fixed(m.ast_build_time_us, 3));
    print_field(out, "ast_nodes", std::to_string(m.ast_nodes));
    print_field(out, "symbol_table_entries", std::to_string(m.symbol_table_entries));
    print_field(out, "thread_validation", m.thread_validation ? "PASS" : "FAIL");
    out << "\n";

    out << "[Phase 4] Unit 4 - DFS Branch & Bound Placement\n";
    print_field(out, "logical_qubits", std::to_string(m.logical_qubits));
    print_field(out, "physical_qubits", std::to_string(m.physical_qubits));
    print_field(out, "lookahead_edges", std::to_string(m.lookahead_edges));
    print_field(out, "greedy_upper_bound_cost", format_fixed(m.greedy_upper_bound_cost, 3));
    print_field(out, "placement_cost_hops", format_fixed(m.placement_cost_hops, 3));
    print_field(out, "nodes_expanded", std::to_string(m.nodes_expanded));
    print_field(out, "branches_pruned", std::to_string(m.branches_pruned));
    print_field(out, "max_stack_depth", std::to_string(m.max_stack_depth));
    print_field(out, "search_completed", m.search_completed ? "true" : "false");
    print_field(out, "placement_time_us", format_fixed(m.placement_time_us, 3));
    out << "\n";

    out << "[Phase 2] Unit 2 - Binary-Heap Dijkstra Routing\n";
    print_field(out, "initial_gate_count", std::to_string(m.initial_gate_count));
    print_field(out, "initial_single_qubit", std::to_string(m.initial_single_qubit_count));
    print_field(out, "initial_two_qubit", std::to_string(m.initial_two_qubit_count));
    print_field(out, "initial_cnot_count", std::to_string(m.initial_cnot_count));
    print_field(out, "initial_measure_count", std::to_string(m.initial_measure_count));
    print_field(out, "initial_barrier_count", std::to_string(m.initial_barrier_count));
    print_field(out, "dijkstra_invocations", std::to_string(m.dijkstra_invocations));
    print_field(out, "inserted_swap_count", std::to_string(m.inserted_swap_count));
    print_field(out, "added_cnot_count", std::to_string(m.added_cnot_count));
    print_field(out, "final_cnot_count", std::to_string(m.final_cnot_count));
    print_field(out, "final_physical_gates", std::to_string(m.final_physical_gate_count));
    print_field(out, "depth_before", std::to_string(m.depth_before));
    print_field(out, "depth_after", std::to_string(m.depth_after));
    print_field(out, "depth_expansion_percent", format_fixed(m.depth_expansion_percent, 2) + "%");
    print_field(out, "cumulative_fidelity", format_fixed(m.cumulative_fidelity, 6));
    print_field(out, "log_infidelity", format_fixed(m.log_infidelity, 6));
    print_field(out, "routing_time_us", format_fixed(m.routing_time_us, 3));
    out << "\n";

    out << "[Phase 3] Unit 3 - MCM Contraction Planning\n";
    print_field(out, "chain_length", std::to_string(m.chain_length));
    print_field(out, "mcm_optimal_cost", m.mcm_overflow_detected ? "SATURATED" : std::to_string(m.mcm_optimal_cost));
    print_field(out, "mcm_left_to_right_cost", m.mcm_overflow_detected ? "SATURATED" : std::to_string(m.mcm_left_to_right_cost));
    print_field(out, "mcm_savings", m.mcm_overflow_detected ? "SATURATED" : std::to_string(m.mcm_savings));
    print_field(out, "mcm_savings_percent", format_fixed(m.mcm_savings_percent, 6) + "%");
    print_field(out, "mcm_overflow_detected", m.mcm_overflow_detected ? "true" : "false");
    print_field(out, "mcm_parenthesization", m.mcm_parenthesization);
    print_field(out, "mcm_plan_time_us", format_fixed(m.mcm_plan_time_us, 3));
    out << "\n";

    out << "[Phase 5] Unit 5 - Fibonacci Heap Telemetry Broker\n";
    print_field(out, "telemetry_inserts", std::to_string(m.telemetry_inserts));
    print_field(out, "telemetry_decrease_keys", std::to_string(m.telemetry_decrease_keys));
    print_field(out, "telemetry_extract_mins", std::to_string(m.telemetry_extract_mins));
    print_field(out, "alerts_dispatched", std::to_string(m.alerts_dispatched));
    print_field(out, "telemetry_time_us", format_fixed(m.telemetry_time_us, 3));
    out << "\n";

    out << "[Summary]\n";
    print_field(out, "total_cpu_time_us", format_fixed(m.total_cpu_time_us, 3));
    print_field(out, "determinism_hash", format_hex64(m.determinism_hash));
    out << "================================================================================\n";
}

void ReportWriter::write_routed_qasm(
    const std::string& path,
    const RoutedCircuit& circuit,
    const unit2::PhysicalCouplingGraph& graph,
    const std::string& circuit_file,
    const std::string& coupling_file,
    const Metrics& metrics)
{
    std::string text = circuit.to_qasm_text(graph, circuit_file, coupling_file, metrics);
    write_text_file(path, text);
}

void ReportWriter::write_json(
    const std::string& path,
    const Metrics& m,
    const std::string& circuit_file,
    const std::string& coupling_file,
    uint32_t lookahead_window,
    const std::vector<int32_t>& l2p,
    const unit3::ContractionPlan& plan,
    const std::vector<unit5::Alert>& alerts)
{
    std::ostringstream j;
    j << "{\n";
    j << "  \"schema_version\": \"1.0\",\n";
    j << "  \"pipeline_revision\": \"R2\",\n";
    j << "  \"inputs\": {\n";
    j << "    \"circuit\": \"" << circuit_file << "\",\n";
    j << "    \"coupling\": \"" << coupling_file << "\",\n";
    j << "    \"lookahead_window\": " << lookahead_window << "\n";
    j << "  },\n";
    j << "  \"metrics\": {\n";
    j << "    \"ast_nodes\": " << m.ast_nodes << ",\n";
    j << "    \"initial_gates\": " << m.initial_gate_count << ",\n";
    j << "    \"final_physical_gates\": " << m.final_physical_gate_count << ",\n";
    j << "    \"inserted_swaps\": " << m.inserted_swap_count << ",\n";
    j << "    \"added_cnots\": " << m.added_cnot_count << ",\n";
    j << "    \"depth_before\": " << m.depth_before << ",\n";
    j << "    \"depth_after\": " << m.depth_after << ",\n";
    j << "    \"depth_expansion_percent\": " << format_fixed(m.depth_expansion_percent, 2) << ",\n";
    j << "    \"cumulative_fidelity\": " << format_fixed(m.cumulative_fidelity, 6) << ",\n";
    j << "    \"log_infidelity\": " << format_fixed(m.log_infidelity, 6) << "\n";
    j << "  },\n";

    j << "  \"placement\": {\n";
    j << "    \"logical_to_physical\": [";
    for (size_t i = 0; i < l2p.size(); ++i) {
        if (i > 0) j << ", ";
        j << l2p[i];
    }
    j << "]\n";
    j << "  },\n";

    j << "  \"contraction_plan\": {\n";
    j << "    \"dimensions\": [";
    for (size_t i = 0; i < plan.dimensions.size(); ++i) {
        if (i > 0) j << ", ";
        j << plan.dimensions[i];
    }
    j << "],\n";
    j << "    \"optimal_cost\": " << plan.optimal_cost << ",\n";
    j << "    \"left_to_right_cost\": " << plan.left_to_right_cost << ",\n";
    j << "    \"savings\": " << plan.savings << ",\n";
    j << "    \"parenthesization\": \"" << plan.parenthesization << "\",\n";
    j << "    \"merge_order\": [";
    for (size_t i = 0; i < plan.merge_order.size(); ++i) {
        if (i > 0) j << ", ";
        j << "[" << plan.merge_order[i].first << ", " << plan.merge_order[i].second << "]";
    }
    j << "],\n";
    j << "    \"overflow_detected\": " << (plan.overflow_detected ? "true" : "false") << "\n";
    j << "  },\n";

    j << "  \"alerts\": [";
    if (!alerts.empty()) {
        j << "\n";
        for (size_t i = 0; i < alerts.size(); ++i) {
            const auto& a = alerts[i];
            j << "    {\n";
            j << "      \"qubit\": " << a.qubit << ",\n";
            j << "      \"slack_us\": " << format_fixed(a.slack_us, 2) << ",\n";
            j << "      \"elapsed_us\": " << format_fixed(a.elapsed_us, 2) << ",\n";
            j << "      \"gate_index\": " << a.gate_index << "\n";
            j << "    }";
            if (i + 1 < alerts.size()) j << ",";
            j << "\n";
        }
        j << "  ";
    }
    j << "],\n";

    j << "  \"timings_us\": {\n";
    j << "    \"parse\": " << format_fixed(m.parse_time_us, 3) << ",\n";
    j << "    \"placement\": " << format_fixed(m.placement_time_us, 3) << ",\n";
    j << "    \"routing\": " << format_fixed(m.routing_time_us, 3) << ",\n";
    j << "    \"mcm\": " << format_fixed(m.mcm_plan_time_us, 3) << ",\n";
    j << "    \"telemetry\": " << format_fixed(m.telemetry_time_us, 3) << ",\n";
    j << "    \"total\": " << format_fixed(m.total_cpu_time_us, 3) << "\n";
    j << "  },\n";

    j << "  \"determinism_hash\": \"" << format_hex64(m.determinism_hash) << "\"\n";
    j << "}\n";

    write_text_file(path, j.str());
}

} // namespace qroute::pipeline
