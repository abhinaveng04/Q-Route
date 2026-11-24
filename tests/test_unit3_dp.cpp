#include "native_assert.hpp"
#include "qroute/common/constants.hpp"
#include "qroute/common/file_io.hpp"
#include "qroute/common/safe_arithmetic.hpp"
#include "qroute/common/types.hpp"
#include "qroute/unit1_ast/qasm_parser.hpp"
#include "qroute/unit3_dp/matrix_chain_optimizer.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

using namespace qroute;
using namespace qroute::unit1;
using namespace qroute::unit3;

// Exhaustive parenthesization brute-force reference oracle strictly inside test file
static uint64_t brute_force_mcm(const std::vector<uint64_t>& p, uint32_t i, uint32_t j) {
    if (i == j) return 0ULL;
    uint64_t min_cost = SafeArithmetic::kMaxU64;
    for (uint32_t k = i; k < j; ++k) {
        uint64_t left = brute_force_mcm(p, i, k);
        uint64_t right = brute_force_mcm(p, k + 1, j);
        uint64_t sub = 0;
        SafeArithmetic::add_u64(left, right, sub);
        uint64_t total = 0;
        SafeArithmetic::saturated_triple_product_add(p[i], p[k + 1], p[j + 1], sub, total);
        if (total < min_cost) {
            min_cost = total;
        }
    }
    return min_cost;
}

// T3.1: Input validation and degenerate chain handling
TEST(Unit3DP, T3_1_Validation_And_DegenerateChain) {
    // Size < 2 throws
    ASSERT_THROW(MatrixChainOptimizer({}), std::invalid_argument);
    ASSERT_THROW(MatrixChainOptimizer({10ULL}), std::invalid_argument);

    // Dimension == 0 throws
    ASSERT_THROW(MatrixChainOptimizer({10ULL, 0ULL, 20ULL}), std::invalid_argument);
    ASSERT_THROW(MatrixChainOptimizer({0ULL, 10ULL}), std::invalid_argument);

    // Degenerate chain: k = 1 (2 dimensions: 1 matrix)
    MatrixChainOptimizer degen({10ULL, 20ULL});
    ContractionPlan plan = degen.build_plan();
    ASSERT_EQ(plan.chain_length, 1U);
    ASSERT_EQ(plan.optimal_cost, 0ULL);
    ASSERT_EQ(plan.left_to_right_cost, 0ULL);
    ASSERT_EQ(plan.savings, 0ULL);
    ASSERT_NEAR(plan.savings_percent, 0.0, 1e-9);
    ASSERT_EQ(plan.parenthesization, "A1");
    ASSERT_TRUE(plan.merge_order.empty());
    ASSERT_FALSE(plan.overflow_detected);
}

// T3.2: Reference CLRS example {10,20,30,40,30} -> 26000 and data/contraction.chain
TEST(Unit3DP, T3_2_CLRS_Reference_Chain) {
    std::vector<uint64_t> dims = {10, 20, 30, 40, 30};
    MatrixChainOptimizer opt(dims);
    ContractionPlan plan = opt.build_plan();

    ASSERT_EQ(plan.chain_length, 4U);
    ASSERT_EQ(plan.optimal_cost, 26000ULL);
    ASSERT_EQ(plan.left_to_right_cost, 41000ULL);
    ASSERT_EQ(plan.savings, 15000ULL);
    ASSERT_NEAR(plan.savings_percent, 36.585366, 1e-4);
    ASSERT_EQ(plan.parenthesization, "((A1(A2A3))A4)");
    ASSERT_FALSE(plan.overflow_detected);

    // Load from data/contraction.chain
    std::vector<uint64_t> loaded_dims = MatrixChainOptimizer::load_chain_file("data/contraction.chain");
    ASSERT_EQ(loaded_dims.size(), 5ULL);
    ASSERT_TRUE(loaded_dims == dims);

    MatrixChainOptimizer loaded_opt(loaded_dims);
    ASSERT_EQ(loaded_opt.optimal_cost(), 26000ULL);
}

// T3.3: Reference CLRS example {30,35,15,5,10,20,25} -> 15125
TEST(Unit3DP, T3_3_CLRS_Large_Example) {
    std::vector<uint64_t> dims = {30, 35, 15, 5, 10, 20, 25};
    MatrixChainOptimizer opt(dims);
    ContractionPlan plan = opt.build_plan();

    ASSERT_EQ(plan.chain_length, 6U);
    ASSERT_EQ(plan.optimal_cost, 15125ULL);
    ASSERT_EQ(plan.parenthesization, "((A1(A2A3))((A4A5)A6))");
    ASSERT_FALSE(plan.overflow_detected);
}

// T3.4: DP == brute force on 200 seeded random chains
TEST(Unit3DP, T3_4_DP_Equals_BruteForce_200_RandomChains) {
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint32_t> k_dist(2, 6);
    std::uniform_int_distribution<uint64_t> dim_dist(5, 100);

    for (int iter = 0; iter < 200; ++iter) {
        uint32_t k = k_dist(rng);
        std::vector<uint64_t> p(k + 1);
        for (uint32_t i = 0; i <= k; ++i) {
            p[i] = dim_dist(rng);
        }

        MatrixChainOptimizer opt(p);
        uint64_t dp_cost = opt.optimal_cost();
        uint64_t bf_cost = brute_force_mcm(p, 0, k - 1);

        ASSERT_EQ(dp_cost, bf_cost);
    }
}

// T3.5: Left-to-right baseline and savings calculation
TEST(Unit3DP, T3_5_LeftToRight_Baseline_And_Savings) {
    std::vector<uint64_t> dims = {30, 35, 15, 5, 10, 20, 25};
    MatrixChainOptimizer opt(dims);
    ContractionPlan plan = opt.build_plan();

    ASSERT_GT(plan.left_to_right_cost, plan.optimal_cost);
    ASSERT_EQ(plan.savings, plan.left_to_right_cost - plan.optimal_cost);
    double expected_pct = 100.0 * static_cast<double>(plan.savings) /
                          static_cast<double>(plan.left_to_right_cost);
    ASSERT_NEAR(plan.savings_percent, expected_pct, 1e-9);
}

// T3.6: Parenthesization string format and merge order
TEST(Unit3DP, T3_6_Parenthesization_And_MergeOrder) {
    std::vector<uint64_t> dims = {10, 20, 30, 40}; // 3 matrices
    MatrixChainOptimizer opt(dims);
    ContractionPlan plan = opt.build_plan();

    ASSERT_EQ(plan.chain_length, 3U);
    ASSERT_FALSE(plan.parenthesization.empty());
    ASSERT_EQ(plan.merge_order.size(), 2ULL); // k - 1 merges
    for (const auto& [i, j] : plan.merge_order) {
        ASSERT_LE(i, j);
        ASSERT_LT(j, 3U);
    }
}

// T3.7: Saturation behavior and overflow detection
TEST(Unit3DP, T3_7_Saturation_And_Overflow_Detection) {
    // Huge dimensions that easily exceed 64-bit integer product:
    // (10^10)^3 = 10^30 >> 2^64
    std::vector<uint64_t> huge_dims = {
        10000000000ULL, 10000000000ULL, 10000000000ULL, 10000000000ULL
    };
    MatrixChainOptimizer opt(huge_dims);
    ContractionPlan plan = opt.build_plan();

    ASSERT_TRUE(opt.overflow_detected());
    ASSERT_TRUE(plan.overflow_detected);
    ASSERT_EQ(plan.optimal_cost, SafeArithmetic::kMaxU64);
    ASSERT_EQ(plan.left_to_right_cost, SafeArithmetic::kMaxU64);
}

// T3.8: INV-3 static gate: no numerical simulation symbols in unit3
TEST(Unit3DP, T3_8_INV3_StaticGate) {
    std::vector<std::string> files = {
        "include/qroute/unit3_dp/matrix_chain_optimizer.hpp",
        "src/unit3_dp/matrix_chain_optimizer.cpp"
    };

    std::vector<std::string> forbidden = {
        "std::complex",
        "blas",
        "lapack",
        "Eigen",
        "state_vector",
        "amplitude"
    };

    for (const auto& file : files) {
        std::string content = read_text_file(file);
        for (const auto& f : forbidden) {
            ASSERT_EQ(content.find(f), std::string::npos);
        }
    }
}

// T3.9: Contiguous row-major layout verification
TEST(Unit3DP, T3_9_Contiguous_RowMajor_Layout) {
    std::vector<uint64_t> dims = {5, 10, 15, 20, 25};
    MatrixChainOptimizer opt(dims);
    uint32_t n = 4;

    const auto& cost = opt.cost_table();
    const auto& split = opt.split_table();
    ASSERT_EQ(cost.size(), static_cast<size_t>(n * n));
    ASSERT_EQ(split.size(), static_cast<size_t>(n * n));

    const uint64_t* cost_ptr = cost.data();
    const uint32_t* split_ptr = split.data();

    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            size_t idx = static_cast<size_t>(i) * n + j;
            ASSERT_EQ(*(cost_ptr + idx), opt.mat(i, j));
            ASSERT_EQ(*(split_ptr + idx), opt.sat(i, j));
        }
    }
}

// T3.10: derive_chain_from_ast determinism across 10 runs
TEST(Unit3DP, T3_10_DeriveChainFromAST_Determinism) {
    ParsedProgram prog = QasmParser::parse_file("data/circuit.qasm");

    std::vector<uint64_t> first_chain = MatrixChainOptimizer::derive_chain_from_ast(prog.ast);
    ASSERT_GE(first_chain.size(), 2ULL);

    for (int run = 0; run < 10; ++run) {
        std::vector<uint64_t> cur_chain = MatrixChainOptimizer::derive_chain_from_ast(prog.ast);
        ASSERT_TRUE(cur_chain == first_chain);
    }
}

// T3.11: Complexity scaling O(k^3): ratio of k=20 vs k=40 within [4x, 16x]
TEST(Unit3DP, T3_11_Complexity_Scaling_O_k3) {
    const uint32_t k1 = 20;
    const uint32_t k2 = 40;

    std::vector<uint64_t> dims1(k1 + 1);
    for (uint32_t i = 0; i <= k1; ++i) dims1[i] = 10 + (i % 7) * 5;

    std::vector<uint64_t> dims2(k2 + 1);
    for (uint32_t i = 0; i <= k2; ++i) dims2[i] = 10 + (i % 7) * 5;

    // Warm up
    {
        MatrixChainOptimizer w1(dims1);
        MatrixChainOptimizer w2(dims2);
        (void)w1.optimal_cost();
        (void)w2.optimal_cost();
    }

    // Run up to 5 trials to avoid OS scheduling / CPU frequency scaling jitter
    bool found_valid = false;
    double last_ratio = 0.0;
    constexpr int kIters = 1000;

    for (int trial = 0; trial < 5; ++trial) {
        auto t0 = std::chrono::steady_clock::now();
        for (int iter = 0; iter < kIters; ++iter) {
            MatrixChainOptimizer opt1(dims1);
            (void)opt1.optimal_cost();
        }
        auto t1 = std::chrono::steady_clock::now();
        double time1 = std::chrono::duration<double, std::micro>(t1 - t0).count();

        auto t2 = std::chrono::steady_clock::now();
        for (int iter = 0; iter < kIters; ++iter) {
            MatrixChainOptimizer opt2(dims2);
            (void)opt2.optimal_cost();
        }
        auto t3 = std::chrono::steady_clock::now();
        double time2 = std::chrono::duration<double, std::micro>(t3 - t2).count();

        last_ratio = time2 / time1;
        if (last_ratio >= 4.0 && last_ratio <= 16.0) {
            found_valid = true;
            break;
        }
    }

    // Expected ratio is ~ (40/20)^3 = 8. Acceptance criterion: within [4x, 16x].
    ASSERT_TRUE(found_valid);
    ASSERT_GE(last_ratio, 4.0);
    ASSERT_LE(last_ratio, 16.0);
}

int main(int argc, char** argv) {
    return qroute::test::run_all_tests(argc, argv);
}
