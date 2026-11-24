#pragma once

#include <cstdint>
#include <string>

namespace qroute::pipeline {

struct Metrics {
    // Phase 1 (Unit 1 - AST)
    double   parse_time_us          = 0.0;
    double   ast_build_time_us      = 0.0;
    uint64_t ast_nodes              = 0;
    uint64_t symbol_table_entries   = 0;
    bool     thread_validation      = false;

    // Phase 4 (Unit 4 - B&B Placement)
    uint32_t logical_qubits         = 0;
    uint32_t physical_qubits        = 0;
    uint64_t lookahead_edges        = 0;
    double   placement_cost_hops    = 0.0;
    double   greedy_upper_bound_cost= 0.0;
    uint64_t nodes_expanded         = 0;
    uint64_t branches_pruned        = 0;
    uint32_t max_stack_depth        = 0;
    bool     search_completed       = false;
    double   placement_time_us      = 0.0;

    // Phase 2 (Unit 2 - Routing)
    uint64_t initial_gate_count         = 0; // single + two-qubit gates
    uint64_t initial_single_qubit_count = 0;
    uint64_t initial_two_qubit_count    = 0;
    uint64_t initial_cnot_count         = 0;
    uint64_t initial_measure_count      = 0;
    uint64_t initial_barrier_count      = 0;
    uint64_t inserted_swap_count        = 0;
    uint64_t added_cnot_count           = 0;
    uint64_t final_cnot_count           = 0;
    uint64_t final_physical_gate_count  = 0;
    uint64_t depth_before               = 0;
    uint64_t depth_after                = 0;
    double   depth_expansion_percent    = 0.0;
    double   cumulative_fidelity        = 1.0;
    double   log_infidelity             = 0.0;
    uint64_t dijkstra_invocations       = 0;
    double   routing_time_us            = 0.0;

    // Phase 3 (Unit 3 - MCM)
    uint32_t    chain_length           = 0;
    uint64_t    mcm_optimal_cost       = 0;
    uint64_t    mcm_left_to_right_cost = 0;
    uint64_t    mcm_savings            = 0;
    double      mcm_savings_percent    = 0.0;
    bool        mcm_overflow_detected  = false;
    std::string mcm_parenthesization;
    double      mcm_plan_time_us       = 0.0;

    // Phase 5 (Unit 5 - Telemetry)
    uint64_t telemetry_inserts       = 0;
    uint64_t telemetry_decrease_keys = 0;
    uint64_t telemetry_extract_mins  = 0;
    uint64_t alerts_dispatched       = 0;
    double   telemetry_time_us       = 0.0;

    // Summary
    double   total_cpu_time_us = 0.0;
    uint64_t determinism_hash  = 0;
};

} // namespace qroute::pipeline
