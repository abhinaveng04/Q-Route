#pragma once

#include "qroute/pipeline/metrics.hpp"
#include "qroute/unit2_graph/physical_coupling_graph.hpp"
#include "qroute/unit3_dp/matrix_chain_optimizer.hpp"
#include "qroute/unit5_heap/telemetry_broker.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace qroute::pipeline {

// Forward declaration of RoutedCircuit
struct RoutedCircuit;

class ReportWriter {
public:
    // D.5 Deterministic stdout report schema
    static void write_stdout(const Metrics& metrics,
                             const std::string& circuit_file,
                             const std::string& coupling_file,
                             std::ostream& out = std::cout);

    // Compute 64-bit FNV-1a hash over all non-timing D.5 fields
    static uint64_t compute_determinism_hash(const Metrics& metrics,
                                             const std::string& circuit_file,
                                             const std::string& coupling_file);

    // D.6 Routed physical circuit OpenQASM 2.0 output
    static void write_routed_qasm(const std::string& path,
                                  const RoutedCircuit& circuit,
                                  const unit2::PhysicalCouplingGraph& graph,
                                  const std::string& circuit_file,
                                  const std::string& coupling_file,
                                  const Metrics& metrics);

    // D.7 Machine-readable JSON report
    static void write_json(const std::string& path,
                           const Metrics& metrics,
                           const std::string& circuit_file,
                           const std::string& coupling_file,
                           uint32_t lookahead_window,
                           const std::vector<int32_t>& l2p,
                           const unit3::ContractionPlan& plan,
                           const std::vector<unit5::Alert>& alerts);
};

} // namespace qroute::pipeline
