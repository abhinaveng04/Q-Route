#include "qroute/unit4_bnb/lookahead_graph.hpp"

#include <algorithm>

namespace qroute::unit4 {

LookaheadGraph::LookaheadGraph(uint32_t logical_qubit_count)
    : n_(logical_qubit_count), logical_count_(logical_qubit_count) {
    matrix_.assign(n_, std::vector<uint32_t>(n_, 0));
    recompute_metadata();
}

void LookaheadGraph::build(const unit1::ThreadedAST& ast, uint32_t window) {
    if (n_ == 0) {
        uint32_t max_q = 0;
        bool found = false;
        ast.traverse([&](NodeIndex /*idx*/, const unit1::ASTNode& node, const unit1::GatePayload& payload) {
            if (is_two_qubit_gate(node.op)) {
                if (payload.qubit_count >= 2) {
                    max_q = std::max({max_q, payload.qubits[0], payload.qubits[1]});
                    found = true;
                }
            } else if (is_single_qubit_gate(node.op) || is_classical_instruction(node.op)) {
                if (payload.qubit_count >= 1) {
                    max_q = std::max(max_q, payload.qubits[0]);
                    found = true;
                }
            }
        });
        n_ = found ? (max_q + 1) : 0;
        logical_count_ = n_;
    }

    matrix_.assign(n_, std::vector<uint32_t>(n_, 0));

    uint32_t two_qubit_count = 0;
    ast.traverse([&](NodeIndex /*idx*/, const unit1::ASTNode& node, const unit1::GatePayload& payload) {
        if (window > 0 && two_qubit_count >= window) {
            return;
        }

        if (is_two_qubit_gate(node.op)) {
            LogicalQubit q0 = payload.qubits[0];
            LogicalQubit q1 = payload.qubits[1];
            ++two_qubit_count;

            if (q0 != q1) { // LG-2: diagonal always 0
                if (q0 >= n_ || q1 >= n_) {
                    uint32_t new_n = std::max(q0, q1) + 1;
                    matrix_.resize(new_n);
                    for (auto& row : matrix_) row.resize(new_n, 0);
                    n_ = new_n;
                    logical_count_ = new_n;
                }
                matrix_[q0][q1] += 1;
                matrix_[q1][q0] += 1;
            }
        }
    });

    recompute_metadata();
}

void LookaheadGraph::build(const unit1::ThreadedAST& ast, uint32_t logical_qubit_count, uint32_t window) {
    n_ = logical_qubit_count;
    logical_count_ = logical_qubit_count;
    build(ast, window);
}

uint32_t LookaheadGraph::weight(LogicalQubit u, LogicalQubit v) const noexcept {
    if (u >= n_ || v >= n_ || u == v) {
        return 0;
    }
    return matrix_[u][v];
}

void LookaheadGraph::add_interaction(LogicalQubit u, LogicalQubit v, uint32_t count) {
    if (u == v || count == 0) return; // LG-2: diagonal always 0
    uint32_t needed = std::max(u, v) + 1;
    if (needed > n_) {
        matrix_.resize(needed);
        for (auto& row : matrix_) row.resize(needed, 0);
        n_ = needed;
        logical_count_ = needed;
    }
    matrix_[u][v] += count;
    matrix_[v][u] += count;
    recompute_metadata();
}

void LookaheadGraph::recompute_metadata() {
    degrees_.assign(n_, 0);
    for (uint32_t i = 0; i < n_; ++i) {
        uint32_t sum = 0;
        for (uint32_t j = 0; j < n_; ++j) {
            sum += matrix_[i][j];
        }
        degrees_[i] = sum;
    }

    degree_sorted_order_.resize(n_);
    for (uint32_t i = 0; i < n_; ++i) {
        degree_sorted_order_[i] = i;
    }

    // LG-4: degree DESC, ties broken by ascending logical id
    std::stable_sort(degree_sorted_order_.begin(), degree_sorted_order_.end(),
        [&](LogicalQubit a, LogicalQubit b) {
            if (degrees_[a] != degrees_[b]) {
                return degrees_[a] > degrees_[b];
            }
            return a < b;
        });

    // LG-3: edges() returned in ascending (i, j) order for deterministic bound evaluation
    edges_.clear();
    for (LogicalQubit i = 0; i < n_; ++i) {
        for (LogicalQubit j = i + 1; j < n_; ++j) {
            if (matrix_[i][j] > 0) {
                edges_.push_back({i, j});
            }
        }
    }
}

} // namespace qroute::unit4
