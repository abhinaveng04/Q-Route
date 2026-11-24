#pragma once

#include "qroute/common/types.hpp"
#include "qroute/unit1_ast/threaded_ast.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace qroute::unit4 {

class LookaheadGraph {
public:
    LookaheadGraph() = default;
    explicit LookaheadGraph(uint32_t logical_qubit_count);

    void build(const unit1::ThreadedAST& ast, uint32_t window = 0);
    void build(const unit1::ThreadedAST& ast, uint32_t logical_qubit_count, uint32_t window);

    uint32_t logical_count() const noexcept { return logical_count_; }
    uint32_t logicalcount() const noexcept { return logical_count(); }

    uint32_t weight(LogicalQubit u, LogicalQubit v) const noexcept;
    uint32_t operator()(LogicalQubit u, LogicalQubit v) const noexcept { return weight(u, v); }

    const std::vector<std::vector<uint32_t>>& matrix() const noexcept { return matrix_; }

    const std::vector<uint32_t>& degree() const noexcept { return degrees_; }

    // Deterministic ordering: degree DESC, then id ASC.
    const std::vector<LogicalQubit>& degree_sorted_order() const noexcept { return degree_sorted_order_; }
    const std::vector<LogicalQubit>& degreesorted_order() const noexcept { return degree_sorted_order(); }

    const std::vector<std::pair<LogicalQubit, LogicalQubit>>& edges() const noexcept { return edges_; }

    // Manual edge addition helper for tests / programmatic setup
    void add_interaction(LogicalQubit u, LogicalQubit v, uint32_t count = 1);
    void addinteraction(LogicalQubit u, LogicalQubit v, uint32_t count = 1) {
        add_interaction(u, v, count);
    }

private:
    uint32_t n_ = 0;
    union {
        uint32_t logical_count_ = 0;
        uint32_t logicalcount_;
    };
    std::vector<std::vector<uint32_t>> matrix_;
    std::vector<uint32_t> degrees_;
    std::vector<LogicalQubit> degree_sorted_order_;
    std::vector<std::pair<LogicalQubit, LogicalQubit>> edges_;

    void recompute_metadata();
};

} // namespace qroute::unit4
