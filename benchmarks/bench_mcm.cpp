#include "qroute/common/deterministic_time.hpp"
#include "qroute/unit3_dp/matrix_chain_optimizer.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace qroute::unit3;

int main() {
    std::cout << "================================================================================\n";
    std::cout << "[B4] MCM: DP vs Left-to-Right\n";
    std::cout << "chain_length,optimal_cost,left_to_right_cost,savings,savings_percent,dp_time_us\n";

    std::vector<uint32_t> chain_lengths = {4, 8, 16, 32, 64};

    for (uint32_t k : chain_lengths) {
        std::vector<uint64_t> dims;
        if (k == 4) {
            dims = {10, 20, 30, 40, 30};
        } else {
            dims.resize(k + 1);
            for (uint32_t i = 0; i <= k; ++i) {
                dims[i] = 10 + (i % 7) * 5;
            }
        }

        // Warm up and timing
        auto t0 = std::chrono::steady_clock::now();
        MatrixChainOptimizer opt(dims);
        ContractionPlan plan = opt.build_plan();
        auto t1 = std::chrono::steady_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        std::cout << plan.chain_length << ","
                  << plan.optimal_cost << ","
                  << plan.left_to_right_cost << ","
                  << plan.savings << ","
                  << std::fixed << std::setprecision(6) << plan.savings_percent << ","
                  << std::setprecision(3) << elapsed_us << "\n";
    }

    std::cout << "================================================================================\n";
    return 0;
}
