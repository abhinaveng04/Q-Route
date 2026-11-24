#pragma once

#include "qroute/common/constants.hpp"
#include "qroute/common/types.hpp"
#include "qroute/unit2_graph/physical_coupling_graph.hpp"
#include "qroute/unit4_bnb/lookahead_graph.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace qroute::unit4 {

struct Frame {
    uint32_t depth = 0;
    PhysicalQubit next_physical = 0;
    LogicalQubit assigned_logical = kNil;
    PhysicalQubit assigned_physical = kNil;
    double assigned_cost = 0.0;
};

struct PlacementResult {
    union {
        double best_cost = kInfinity;
        double bestcost;
    };
    std::vector<int32_t> logical_to_physical; // -1 == unmapped
    const std::vector<int32_t>& logicaltophysical() const noexcept { return logical_to_physical; }
    std::vector<int32_t>& logicaltophysical() noexcept { return logical_to_physical; }

    union {
        uint64_t nodes_expanded = 0;
        uint64_t nodesexpanded;
    };
    union {
        uint64_t branches_pruned = 0;
        uint64_t branchespruned;
    };
    union {
        uint32_t max_stack_depth = 0;
        uint32_t maxstackdepth;
    };
    bool completed = false; // false if aborted by time limit or node budget
    bool feasible = false;
    union {
        double search_time_us = 0.0;
        double searchtimeus;
    };
    union {
        double greedy_upper_bound_cost = kInfinity;
        double greedyupperbound_cost;
    };
};

struct PlacementConfig {
    union {
        uint32_t logical_count = 0;
        uint32_t logicalcount;
    };
    union {
        uint32_t physical_count = 0;
        uint32_t physicalcount;
    };
    const unit2::PhysicalCouplingGraph* graph = nullptr; // non-owning, must outlive solver
    LookaheadGraph lookahead;
    union {
        double time_limit_ms = 1000.0;
        double timelimitms;
    };
    union {
        uint64_t node_budget = std::numeric_limits<uint64_t>::max();
        uint64_t nodebudget;
    };
    union {
        bool use_hop_distance = true;
        bool usehopdistance;
    };
};

class QubitPlacementBAndB {
public:
    explicit QubitPlacementBAndB(PlacementConfig cfg);

    PlacementResult solve();

    const std::vector<int32_t>& best_assignment() const noexcept { return best_assignment_; }
    const std::vector<int32_t>& bestassignment() const noexcept { return best_assignment(); }

    uint32_t max_stack_depth() const noexcept { return max_stack_depth_; }
    uint32_t maxstackdepth() const noexcept { return max_stack_depth(); }

    double greedy_upper_bound();
    double greedyupperbound() { return greedy_upper_bound(); }

    double lower_bound(double assigned_cost) const;
    double lowerbound(double assigned_cost) const { return lower_bound(assigned_cost); }

    void precompute_bound_structures();
    void precomputeboundstructures() { precompute_bound_structures(); }

    double assign_incremental_cost(uint32_t logical_qubit, uint32_t physical_qubit) const;
    double assignincrementalcost(uint32_t logical_qubit, uint32_t physical_qubit) const {
        return assign_incremental_cost(logical_qubit, physical_qubit);
    }

    double min_distance_to_unused(uint32_t physical_qubit) const;
    double mindistancetounused(uint32_t physical_qubit) const {
        return min_distance_to_unused(physical_qubit);
    }

    void assign(uint32_t logical_qubit, uint32_t physical_qubit);
    void undo_assign(uint32_t logical_qubit, uint32_t physical_qubit);
    void undoassign(uint32_t logical_qubit, uint32_t physical_qubit) {
        undo_assign(logical_qubit, physical_qubit);
    }

    bool budget_exhausted() const noexcept;
    bool budgetexhausted() const noexcept { return budget_exhausted(); }

    const PlacementConfig& config() const noexcept { return cfg_; }

    double distance(PhysicalQubit u, PhysicalQubit v) const;

private:
    PlacementConfig cfg_;
    std::vector<LogicalQubit> variable_order_;
    std::vector<int32_t> current_l2p_;
    std::vector<uint8_t> current_used_;
    std::vector<Frame> stack_;

    double best_cost_ = kInfinity;
    std::vector<int32_t> best_assignment_;

    uint64_t nodes_expanded_ = 0;
    uint64_t branches_pruned_ = 0;
    uint32_t max_stack_depth_ = 0;

    std::vector<std::vector<std::pair<double, PhysicalQubit>>> sorted_dist_from_;
    double min_pair_distance_ = 1.0;

    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point deadline_;
};

} // namespace qroute::unit4
