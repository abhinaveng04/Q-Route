#pragma once

#include "qroute/common/safe_arithmetic.hpp"
#include "qroute/common/types.hpp"
#include "qroute/unit1_ast/threaded_ast.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace qroute::unit3 {

struct ContractionPlan {
    uint32_t chain_length = 0; // k matrices
    std::vector<uint64_t> dimensions; // p[0..k]

    uint64_t optimal_cost = 0; // m[0][k-1]
    uint64_t left_to_right_cost = 0;
    uint64_t savings = 0; // LR - optimal (0 if LR <= optimal)
    double savings_percent = 0.0;

    std::string parenthesization;
    std::vector<std::pair<uint32_t, uint32_t>> merge_order; // (i, j) pairs in execution order

    std::vector<uint64_t> cost_table;  // flattened k x k row-major (m)
    std::vector<uint32_t> split_table; // flattened k x k row-major (s)

    bool overflow_detected = false;
};

class MatrixChainOptimizer {
public:
    // dimensions.size() must be >= 2; dimensions[i] > 0 for all i.
    explicit MatrixChainOptimizer(std::vector<uint64_t> dimensions);

    ContractionPlan build_plan();
    ContractionPlan buildplan() { return build_plan(); }

    uint64_t optimal_cost() const noexcept;
    uint64_t optimalcost() const noexcept { return optimal_cost(); }

    const std::vector<uint64_t>& cost_table() const noexcept { return m_; }
    const std::vector<uint64_t>& costtable() const noexcept { return cost_table(); }

    const std::vector<uint32_t>& split_table() const noexcept { return s_; }
    const std::vector<uint32_t>& splittable() const noexcept { return split_table(); }

    bool overflow_detected() const noexcept { return overflow_; }
    bool overflowdetected() const noexcept { return overflow_detected(); }

    // Deterministic chain derivation from a parsed circuit (see B.3.6)
    static std::vector<uint64_t> derive_chain_from_ast(const unit1::ThreadedAST& ast);
    static std::vector<uint64_t> derivechainfromast(const unit1::ThreadedAST& ast) {
        return derive_chain_from_ast(ast);
    }
    static std::vector<uint64_t> derivechainfrom_ast(const unit1::ThreadedAST& ast) {
        return derive_chain_from_ast(ast);
    }

    static std::vector<uint64_t> load_chain_file(const std::string& path);
    static std::vector<uint64_t> loadchain_file(const std::string& path) {
        return load_chain_file(path);
    }
    static std::vector<uint64_t> loadchainfile(const std::string& path) {
        return load_chain_file(path);
    }

    uint64_t& m(uint32_t i, uint32_t j) noexcept;
    uint32_t& s(uint32_t i, uint32_t j) noexcept;
    uint64_t  mat(uint32_t i, uint32_t j) const noexcept;
    uint32_t  sat(uint32_t i, uint32_t j) const noexcept;

private:
    std::vector<uint64_t> p_;
    uint32_t              n_ = 0; // matrix count = p_.size() - 1
    std::vector<uint64_t> m_;    // n * n, row-major
    std::vector<uint32_t> s_;    // n * n, row-major
    bool                  overflow_ = false;

    void run_dp();
    uint64_t compute_left_to_right_cost();
    void build_parenthesization(uint32_t i, uint32_t j, std::string& out) const;
    void build_merge_order(uint32_t i, uint32_t j,
                           std::vector<std::pair<uint32_t, uint32_t>>& out) const;
};

} // namespace qroute::unit3
