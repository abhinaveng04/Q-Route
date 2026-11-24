#include "qroute/unit4_bnb/qubit_placement_bnb.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace qroute::unit4 {

QubitPlacementBAndB::QubitPlacementBAndB(PlacementConfig cfg)
    : cfg_(std::move(cfg)) {
    const uint32_t n = cfg_.logical_count;
    const uint32_t m = cfg_.physical_count;
    current_l2p_.assign(n, -1);
    current_used_.assign(m, 0);
    variable_order_ = cfg_.lookahead.degree_sorted_order();
    if (variable_order_.size() < n) {
        variable_order_.resize(n);
        for (uint32_t i = 0; i < n; ++i) variable_order_[i] = i;
    }
}

double QubitPlacementBAndB::distance(PhysicalQubit u, PhysicalQubit v) const {
    if (u == v) return 0.0;
    if (!cfg_.graph) return kInfinity;
    if (cfg_.use_hop_distance) {
        uint16_t h = cfg_.graph->hops(u, v);
        if (h == std::numeric_limits<uint16_t>::max()) return kInfinity;
        return static_cast<double>(h);
    } else {
        return cfg_.graph->weighted_distance(u, v);
    }
}

void QubitPlacementBAndB::precompute_bound_structures() {
    const uint32_t m = cfg_.physical_count;
    sorted_dist_from_.clear();
    sorted_dist_from_.resize(m);

    min_pair_distance_ = kInfinity;

    for (PhysicalQubit p = 0; p < m; ++p) {
        sorted_dist_from_[p].reserve(m);
        for (PhysicalQubit p2 = 0; p2 < m; ++p2) {
            double d = distance(p, p2);
            sorted_dist_from_[p].push_back({d, p2});
            if (p != p2 && d < min_pair_distance_) {
                min_pair_distance_ = d;
            }
        }
        std::sort(sorted_dist_from_[p].begin(), sorted_dist_from_[p].end(),
            [](const std::pair<double, PhysicalQubit>& a, const std::pair<double, PhysicalQubit>& b) {
                if (std::abs(a.first - b.first) > kEpsilon) {
                    return a.first < b.first;
                }
                return a.second < b.second;
            });
    }

    if (min_pair_distance_ >= kInfinity) {
        min_pair_distance_ = 1.0;
    }
}

double QubitPlacementBAndB::greedy_upper_bound() {
    const uint32_t n = cfg_.logical_count;
    const uint32_t m = cfg_.physical_count;
    if (n == 0) {
        best_cost_ = 0.0;
        best_assignment_.clear();
        return 0.0;
    }
    if (n > m) {
        best_cost_ = kInfinity;
        best_assignment_.assign(n, -1);
        return kInfinity;
    }

    if (variable_order_.empty()) {
        variable_order_ = cfg_.lookahead.degree_sorted_order();
        if (variable_order_.size() < n) {
            variable_order_.resize(n);
            for (uint32_t i = 0; i < n; ++i) variable_order_[i] = i;
        }
    }

    std::vector<int32_t> greedy_l2p(n, -1);
    std::vector<uint8_t> greedy_used(m, 0);
    double total_cost = 0.0;

    for (LogicalQubit l : variable_order_) {
        PhysicalQubit best_p = 0;
        double min_inc = kInfinity;

        for (PhysicalQubit p = 0; p < m; ++p) {
            if (greedy_used[p]) continue;

            double inc = 0.0;
            bool unreachable = false;
            for (LogicalQubit l2 = 0; l2 < n; ++l2) {
                if (l2 == l) continue;
                int32_t p2 = greedy_l2p[l2];
                if (p2 != -1) {
                    uint32_t alpha = cfg_.lookahead.weight(l, l2);
                    if (alpha > 0) {
                        double d = distance(p, static_cast<PhysicalQubit>(p2));
                        if (d >= kInfinity) {
                            unreachable = true;
                            break;
                        }
                        inc += static_cast<double>(alpha) * d;
                    }
                }
            }

            if (unreachable) {
                inc = kInfinity;
            }

            if (inc < min_inc) {
                min_inc = inc;
                best_p = p;
            }
        }

        greedy_l2p[l] = static_cast<int32_t>(best_p);
        greedy_used[best_p] = 1;
        if (min_inc >= kInfinity) {
            total_cost = kInfinity;
        } else if (total_cost < kInfinity) {
            total_cost += min_inc;
        }
    }

    best_cost_ = total_cost;
    best_assignment_ = greedy_l2p;
    return best_cost_;
}

double QubitPlacementBAndB::assign_incremental_cost(uint32_t logical_qubit, uint32_t physical_qubit) const {
    double cost = 0.0;
    const uint32_t n = cfg_.logical_count;
    for (uint32_t l2 = 0; l2 < n; ++l2) {
        if (l2 == logical_qubit) continue;
        int32_t p2 = current_l2p_[l2];
        if (p2 != -1) {
            uint32_t alpha = cfg_.lookahead.weight(logical_qubit, l2);
            if (alpha > 0) {
                double d = distance(physical_qubit, static_cast<PhysicalQubit>(p2));
                if (d >= kInfinity) return kInfinity;
                cost += static_cast<double>(alpha) * d;
            }
        }
    }
    return cost;
}

double QubitPlacementBAndB::min_distance_to_unused(uint32_t physical_qubit) const {
    if (physical_qubit >= sorted_dist_from_.size()) return kInfinity;
    const auto& list = sorted_dist_from_[physical_qubit];
    for (const auto& entry : list) {
        if (!current_used_[entry.second]) {
            return entry.first;
        }
    }
    return kInfinity;
}

double QubitPlacementBAndB::lower_bound(double assigned_cost) const {
    double lb = assigned_cost;
    for (const auto& edge : cfg_.lookahead.edges()) {
        LogicalQubit i = edge.first;
        LogicalQubit j = edge.second;

        bool i_assigned = (current_l2p_[i] != -1);
        bool j_assigned = (current_l2p_[j] != -1);

        if (i_assigned && j_assigned) {
            continue;
        }

        uint32_t alpha = cfg_.lookahead.weight(i, j);
        if (alpha == 0) continue;

        double delta = 0.0;
        if (i_assigned && !j_assigned) {
            delta = min_distance_to_unused(static_cast<PhysicalQubit>(current_l2p_[i]));
        } else if (!i_assigned && j_assigned) {
            delta = min_distance_to_unused(static_cast<PhysicalQubit>(current_l2p_[j]));
        } else {
            delta = min_pair_distance_;
        }

        if (delta >= kInfinity) {
            return kInfinity;
        }
        lb += static_cast<double>(alpha) * delta;
    }
    return lb;
}

void QubitPlacementBAndB::assign(uint32_t logical_qubit, uint32_t physical_qubit) {
    current_l2p_[logical_qubit] = static_cast<int32_t>(physical_qubit);
    current_used_[physical_qubit] = 1;
}

void QubitPlacementBAndB::undo_assign(uint32_t logical_qubit, uint32_t physical_qubit) {
    current_l2p_[logical_qubit] = -1;
    current_used_[physical_qubit] = 0;
}

bool QubitPlacementBAndB::budget_exhausted() const noexcept {
    if (cfg_.node_budget > 0 && nodes_expanded_ >= cfg_.node_budget) {
        return true;
    }
    if (cfg_.time_limit_ms > 0.0) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline_) {
            return true;
        }
    }
    return false;
}

PlacementResult QubitPlacementBAndB::solve() {
    start_time_ = std::chrono::steady_clock::now();
    deadline_ = start_time_ + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double, std::milli>(cfg_.time_limit_ms));

    nodes_expanded_ = 0;
    branches_pruned_ = 0;
    max_stack_depth_ = 0;

    const uint32_t n = cfg_.logical_count;
    const uint32_t m = cfg_.physical_count;

    PlacementResult res;

    if (n == 0) {
        res.best_cost = 0.0;
        res.greedy_upper_bound_cost = 0.0;
        res.logical_to_physical.clear();
        res.nodes_expanded = 0;
        res.branches_pruned = 0;
        res.max_stack_depth = 0;
        res.completed = true;
        res.feasible = true;
        auto end_time = std::chrono::steady_clock::now();
        res.search_time_us = std::chrono::duration<double, std::micro>(end_time - start_time_).count();
        assert(max_stack_depth_ <= n + 1);
        return res;
    }

    if (n > m || cfg_.graph == nullptr) {
        res.best_cost = kInfinity;
        res.greedy_upper_bound_cost = kInfinity;
        res.logical_to_physical.assign(n, -1);
        res.nodes_expanded = 0;
        res.branches_pruned = 0;
        res.max_stack_depth = 0;
        res.completed = true;
        res.feasible = false;
        auto end_time = std::chrono::steady_clock::now();
        res.search_time_us = std::chrono::duration<double, std::micro>(end_time - start_time_).count();
        assert(max_stack_depth_ <= n + 1);
        return res;
    }

    precompute_bound_structures();
    variable_order_ = cfg_.lookahead.degree_sorted_order();
    if (variable_order_.size() < n) {
        variable_order_.resize(n);
        for (uint32_t i = 0; i < n; ++i) variable_order_[i] = i;
    }

    double greedy_cost = greedy_upper_bound();
    res.greedy_upper_bound_cost = greedy_cost;
    best_cost_ = greedy_cost;

    current_l2p_.assign(n, -1);
    current_used_.assign(m, 0);

    stack_.clear();
    stack_.reserve(n + 1);
    stack_.push_back(Frame{0, 0, kNil, kNil, 0.0});
    nodes_expanded_ = 1;

    bool aborted = false;

    while (!stack_.empty()) {
        Frame& f = stack_.back();
        max_stack_depth_ = std::max(max_stack_depth_, static_cast<uint32_t>(stack_.size()));

        if (budget_exhausted()) {
            aborted = true;
            break;
        }

        if (f.depth == n) {
            if (f.assigned_cost < best_cost_ - kEpsilon) {
                best_cost_ = f.assigned_cost;
                best_assignment_ = current_l2p_;
            }
            if (f.assigned_logical != kNil) {
                undo_assign(f.assigned_logical, f.assigned_physical);
            }
            stack_.pop_back();
            continue;
        }

        if (f.next_physical >= m) {
            if (f.assigned_logical != kNil) {
                undo_assign(f.assigned_logical, f.assigned_physical);
            }
            stack_.pop_back();
            continue;
        }

        PhysicalQubit p = f.next_physical++;
        if (current_used_[p]) {
            continue;
        }

        LogicalQubit var = variable_order_[f.depth];
        double inc_cost = assign_incremental_cost(var, p);
        if (inc_cost >= kInfinity) {
            ++branches_pruned_;
            continue;
        }

        double new_cost = f.assigned_cost + inc_cost;
        assign(var, p);

        double lb = lower_bound(new_cost);
        if (lb >= best_cost_ - kEpsilon) {
            ++branches_pruned_;
            undo_assign(var, p);
            continue;
        }

        ++nodes_expanded_;
        stack_.push_back(Frame{f.depth + 1, 0, var, p, new_cost});
    }

    auto end_time = std::chrono::steady_clock::now();
    res.search_time_us = std::chrono::duration<double, std::micro>(end_time - start_time_).count();

    res.best_cost = best_cost_;
    res.logical_to_physical = best_assignment_;
    res.nodes_expanded = nodes_expanded_;
    res.branches_pruned = branches_pruned_;
    res.max_stack_depth = max_stack_depth_;
    res.completed = !aborted;
    res.feasible = (best_cost_ < kInfinity);

    assert(max_stack_depth_ <= n + 1);

    return res;
}

} // namespace qroute::unit4
