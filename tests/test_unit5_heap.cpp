#include "native_assert.hpp"
#include "qroute/common/constants.hpp"
#include "qroute/common/file_io.hpp"
#include "qroute/unit5_heap/binary_heap_baseline.hpp"
#include "qroute/unit5_heap/fibonacci_heap.hpp"
#include "qroute/unit5_heap/telemetry_broker.hpp"
#include "qroute/unit5_heap/telemetry_config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace qroute;
using namespace qroute::unit5;

// T5.1: insert/minimum/extract_min on monotonic, reverse, and random sequences
TEST(Unit5Heap, T5_1_Monotonic_Reverse_Random) {
    // 1. Monotonic ascending
    {
        FibonacciHeap<int, int> heap;
        for (int i = 0; i < 100; ++i) {
            heap.insert(i, i);
            ASSERT_EQ(heap.minimum().key, 0);
        }
        ASSERT_EQ(heap.size(), 100ULL);
        for (int i = 0; i < 100; ++i) {
            auto [k, v] = heap.extract_min();
            ASSERT_EQ(k, i);
            ASSERT_EQ(v, i);
        }
        ASSERT_TRUE(heap.empty());
    }

    // 2. Reverse descending
    {
        FibonacciHeap<int, int> heap;
        for (int i = 99; i >= 0; --i) {
            heap.insert(i, i);
            ASSERT_EQ(heap.minimum().key, i);
        }
        ASSERT_EQ(heap.size(), 100ULL);
        for (int i = 0; i < 100; ++i) {
            auto [k, v] = heap.extract_min();
            ASSERT_EQ(k, i);
            ASSERT_EQ(v, i);
        }
        ASSERT_TRUE(heap.empty());
    }

    // 3. Random sequence
    {
        std::mt19937 rng(1337);
        std::vector<int> vals(200);
        for (int i = 0; i < 200; ++i) {
            vals[i] = i * 3 - 50;
        }
        std::shuffle(vals.begin(), vals.end(), rng);

        FibonacciHeap<int, int> heap;
        for (int x : vals) {
            heap.insert(x, x);
        }
        ASSERT_EQ(heap.size(), 200ULL);

        std::sort(vals.begin(), vals.end());
        for (int expected : vals) {
            ASSERT_EQ(heap.minimum().key, expected);
            auto [k, v] = heap.extract_min();
            ASSERT_EQ(k, expected);
            ASSERT_EQ(v, expected);
        }
        ASSERT_TRUE(heap.empty());
    }
}

// T5.2: validate_structure() after 100k seeded mixed ops
TEST(Unit5Heap, T5_2_ValidateStructure_100k_MixedOps) {
    std::mt19937_64 rng(42);
    FibonacciHeap<int64_t, uint32_t> heap;
    std::vector<FibonacciHeap<int64_t, uint32_t>::Handle> active_handles;

    std::uniform_int_distribution<int> op_dist(0, 99);
    std::uniform_int_distribution<int64_t> key_dist(1000, 1000000);

    constexpr int kTotalOps = 100000;

    for (int op = 0; op < kTotalOps; ++op) {
        int choice = op_dist(rng);

        if (active_handles.empty() || choice < 45) {
            // 45% Insert
            int64_t k = key_dist(rng);
            auto h = heap.insert(k, static_cast<uint32_t>(op));
            active_handles.push_back(h);
        } else if (choice < 80) {
            // 35% Decrease key
            size_t idx = static_cast<size_t>(rng() % active_handles.size());
            auto h = active_handles[idx];
            if (heap.contains_handle(h)) {
                int64_t cur_key = h->key;
                int64_t delta = static_cast<int64_t>(rng() % 500) + 1;
                heap.decrease_key(h, cur_key - delta);
            }
        } else {
            // 20% Extract min
            if (!heap.empty()) {
                auto min_h = const_cast<FibonacciHeap<int64_t, uint32_t>::Node*>(&heap.minimum());
                heap.extract_min();
                auto it = std::find(active_handles.begin(), active_handles.end(), min_h);
                if (it != active_handles.end()) {
                    *it = active_handles.back();
                    active_handles.pop_back();
                }
            }
        }

        // Validate structure periodically to keep test runtime snappy while thoroughly checking
        if (op % 1000 == 0 || op == kTotalOps - 1) {
            ASSERT_TRUE(heap.validate_structure());
        }
    }

    ASSERT_TRUE(heap.validate_structure());
}

// T5.3: Equivalence vs BinaryHeapBaseline and vs std::priority_queue
TEST(Unit5Heap, T5_3_Equivalence_vs_BinaryHeap_and_StdPQ) {
    std::mt19937_64 rng(101);

    FibonacciHeap<int64_t, uint32_t> fib_heap;
    BinaryHeapBaseline<int64_t, uint32_t> bin_heap;
    // std::priority_queue min-heap for simple insert/extract stream
    using PQEntry = std::pair<int64_t, uint32_t>;
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> std_pq;

    std::uniform_int_distribution<int> op_dist(0, 99);
    std::uniform_int_distribution<int64_t> key_dist(1, 100000);

    constexpr int kOps = 5000;
    for (int i = 0; i < kOps; ++i) {
        int choice = op_dist(rng);
        if (fib_heap.empty() || choice < 60) {
            int64_t k = key_dist(rng);
            uint32_t v = static_cast<uint32_t>(i);
            fib_heap.insert(k, v);
            bin_heap.insert(k, v);
            std_pq.push({k, v});
        } else {
            ASSERT_EQ(fib_heap.size(), bin_heap.size());
            ASSERT_EQ(fib_heap.size(), std_pq.size());

            auto [fk, fv] = fib_heap.extract_min();
            auto [bk, bv] = bin_heap.extract_min();
            auto [pk, pv] = std_pq.top();
            std_pq.pop();

            ASSERT_EQ(fk, bk);
            ASSERT_EQ(fk, pk);
            (void)fv; (void)bv; (void)pv;
        }
    }

    // Drain remaining
    while (!fib_heap.empty()) {
        auto [fk, fv] = fib_heap.extract_min();
        auto [bk, bv] = bin_heap.extract_min();
        auto [pk, pv] = std_pq.top();
        std_pq.pop();

        ASSERT_EQ(fk, bk);
        ASSERT_EQ(fk, pk);
        (void)fv; (void)bv; (void)pv;
    }
    ASSERT_TRUE(bin_heap.empty());
    ASSERT_TRUE(std_pq.empty());
}

// T5.4: Cascading-cut structural test (explicit mark/cut sequence)
TEST(Unit5Heap, T5_4_CascadingCut_StructuralTest) {
    FibonacciHeap<int, int> heap;
    std::vector<FibonacciHeap<int, int>::Handle> handles;

    // Insert 9 elements: 0..8
    for (int i = 0; i < 9; ++i) {
        handles.push_back(heap.insert(i * 10, i));
    }

    // Extract minimum (0). Remaining 8 elements (10, 20, 30, 40, 50, 60, 70, 80)
    // are consolidated into a single B_3 binomial tree rooted at 10.
    heap.extract_min();
    ASSERT_TRUE(heap.validate_structure());

    // In a B_3 tree, there is a child y of root that has degree >= 2
    FibonacciHeap<int, int>::Handle y = nullptr;
    for (size_t i = 1; i < handles.size(); ++i) {
        if (handles[i]->parent != nullptr && handles[i]->degree >= 2) {
            y = handles[i];
            break;
        }
    }
    ASSERT_TRUE(y != nullptr);
    ASSERT_GE(y->degree, 2);

    auto c1 = y->child;
    ASSERT_TRUE(c1 != nullptr);
    auto c2 = c1->right;
    ASSERT_TRUE(c2 != nullptr && c2 != c1);

    uint64_t cuts_before = heap.stats().cuts;
    uint64_t cas_before = heap.stats().cascading_cuts;

    // Cut 1: decrease c1 below y's key. Causes cut(c1, y) and marks y.
    heap.decrease_key(c1, y->key - 1);
    ASSERT_GT(heap.stats().cuts, cuts_before);
    ASSERT_TRUE(y->mark);

    // Cut 2: decrease c2 below y's key. Since y is already marked, cascading_cut(y) triggers!
    heap.decrease_key(c2, y->key - 2);
    ASSERT_GT(heap.stats().cascading_cuts, cas_before);
    ASSERT_TRUE(heap.validate_structure());
}

// T5.5: Degree bound invariant
TEST(Unit5Heap, T5_5_DegreeBound_Invariant) {
    std::mt19937 rng(999);
    FibonacciHeap<int, int> heap;

    double phi = (1.0 + std::sqrt(5.0)) / 2.0;

    for (int i = 0; i < 500; ++i) {
        heap.insert(rng() % 10000, i);
        if (i % 5 == 0 && !heap.empty()) {
            heap.extract_min();
        }

        if (heap.size() > 0) {
            uint32_t bound = static_cast<uint32_t>(
                std::floor(std::log(static_cast<double>(heap.size())) / std::log(phi))) + 1;
            ASSERT_LE(heap.max_degree(), bound);
        }
    }
    ASSERT_TRUE(heap.validate_structure());
}

// T5.6: Pool/free-list recycling bound
TEST(Unit5Heap, T5_6_PoolRecycling_Bound) {
    FibonacciHeap<int, int> heap;
    constexpr int N = 1000;

    // First wave: insert N, pool size becomes N
    for (int i = 0; i < N; ++i) {
        heap.insert(i, i);
    }
    ASSERT_EQ(heap.size(), static_cast<size_t>(N));
    ASSERT_EQ(heap.pool_size(), static_cast<size_t>(N));
    ASSERT_EQ(heap.free_list_size(), 0ULL);

    // Extract all N: free list becomes N
    for (int i = 0; i < N; ++i) {
        heap.extract_min();
    }
    ASSERT_TRUE(heap.empty());
    ASSERT_EQ(heap.free_list_size(), static_cast<size_t>(N));

    // Second wave: insert N again. Must reuse from free list, pool must NOT grow!
    for (int i = 0; i < N; ++i) {
        heap.insert(i + 100, i);
    }
    ASSERT_EQ(heap.size(), static_cast<size_t>(N));
    ASSERT_EQ(heap.pool_size(), static_cast<size_t>(N));
    ASSERT_EQ(heap.stats().nodes_recycled, static_cast<size_t>(N));
    ASSERT_EQ(heap.free_list_size(), 0ULL);
}

// T5.7: Handle invalidation semantics
TEST(Unit5Heap, T5_7_HandleInvalidation) {
    FibonacciHeap<int, int> heap;
    auto h1 = heap.insert(10, 1);
    auto h2 = heap.insert(20, 2);

    ASSERT_TRUE(heap.contains_handle(h1));
    ASSERT_TRUE(heap.contains_handle(h2));

    auto [k, v] = heap.extract_min(); // extracts h1 (key 10)
    ASSERT_EQ(k, 10);
    ASSERT_EQ(v, 1);

    // h1 is now invalidated
    ASSERT_FALSE(heap.contains_handle(h1));
    ASSERT_TRUE(heap.contains_handle(h2));

    // decrease_key on invalid handle must be a safe no-op
    uint64_t dec_before = heap.stats().decrease_keys;
    heap.decrease_key(h1, 5);
    ASSERT_EQ(heap.stats().decrease_keys, dec_before);
    ASSERT_EQ(heap.minimum().key, 20); // unaffected
}

// T5.8: ASan/LSan clean run & RAII lifetime
TEST(Unit5Heap, T5_8_RAII_Lifetime) {
    {
        FibonacciHeap<int, std::string> heap;
        for (int i = 0; i < 200; ++i) {
            heap.insert(i, "val_" + std::to_string(i));
        }
        for (int i = 0; i < 50; ++i) {
            heap.extract_min();
        }
    } // Heap leaves scope, pool_ cleanly deallocates
    ASSERT_TRUE(true);
}

// T5.9: absorb() correctness
TEST(Unit5Heap, T5_9_Absorb_Correctness) {
    FibonacciHeap<int, int> heap1;
    FibonacciHeap<int, int> heap2;

    std::vector<int> vals1 = {15, 3, 9, 21, 7};
    std::vector<int> vals2 = {8, 1, 30, 12};

    for (int x : vals1) heap1.insert(x, x);
    for (int x : vals2) heap2.insert(x, x);

    ASSERT_EQ(heap1.size(), 5ULL);
    ASSERT_EQ(heap2.size(), 4ULL);

    heap1.absorb(heap2);

    ASSERT_EQ(heap1.size(), 9ULL);
    ASSERT_TRUE(heap2.empty());
    ASSERT_EQ(heap2.size(), 0ULL);
    ASSERT_TRUE(heap1.validate_structure());

    std::vector<int> all_vals;
    all_vals.insert(all_vals.end(), vals1.begin(), vals1.end());
    all_vals.insert(all_vals.end(), vals2.begin(), vals2.end());
    std::sort(all_vals.begin(), all_vals.end());

    for (int exp : all_vals) {
        auto [k, v] = heap1.extract_min();
        ASSERT_EQ(k, exp);
        ASSERT_EQ(v, exp);
    }
    ASSERT_TRUE(heap1.empty());
}

// T5.10: Benchmark workload simulation
TEST(Unit5Heap, T5_10_Telemetry_and_Routing_Workloads) {
    std::mt19937 rng(777);

    // 1. Telemetry-like workload: 20% insert, 75% decrease-key, 5% extract-min
    {
        FibonacciHeap<double, uint32_t> fib;
        std::vector<FibonacciHeap<double, uint32_t>::Handle> handles;
        for (uint32_t i = 0; i < 50; ++i) {
            handles.push_back(fib.insert(100.0 + i, i));
        }

        for (int iter = 0; iter < 10000; ++iter) {
            int p = rng() % 100;
            if (p < 20 || handles.empty()) {
                uint32_t q = static_cast<uint32_t>(handles.size());
                handles.push_back(fib.insert(50.0 + (rng() % 1000) * 0.1, q));
            } else if (p < 95) {
                size_t idx = rng() % handles.size();
                auto h = handles[idx];
                if (fib.contains_handle(h)) {
                    fib.decrease_key(h, h->key * 0.95);
                }
            } else {
                if (!fib.empty()) {
                    auto [k, v] = fib.extract_min();
                    (void)k; (void)v;
                }
            }
        }
        ASSERT_TRUE(fib.validate_structure());
    }

    // 2. Routing-like workload: 70% insert, 5% decrease-key, 25% extract-min
    {
        FibonacciHeap<double, uint32_t> fib;
        std::vector<FibonacciHeap<double, uint32_t>::Handle> handles;
        for (int iter = 0; iter < 10000; ++iter) {
            int p = rng() % 100;
            if (p < 70 || handles.empty()) {
                uint32_t q = static_cast<uint32_t>(handles.size());
                handles.push_back(fib.insert(10.0 + (rng() % 500) * 0.2, q));
            } else if (p < 75) {
                size_t idx = rng() % handles.size();
                auto h = handles[idx];
                if (fib.contains_handle(h)) {
                    fib.decrease_key(h, h->key * 0.9);
                }
            } else {
                if (!fib.empty()) {
                    auto [k, v] = fib.extract_min();
                    (void)k; (void)v;
                }
            }
        }
        ASSERT_TRUE(fib.validate_structure());
    }
}

// T5.11: Slack formula verification against closed-form computation
TEST(Unit5Heap, T5_11_SlackFormula_ClosedForm) {
    TelemetryConfig cfg;
    cfg.t1_us = 50.0;
    cfg.t2_us = 70.0;
    cfg.fidelity_threshold = 0.90;
    cfg.alert_slack_us = 5.0;

    TelemetryBroker broker(4, cfg);

    // Closed-form calculation:
    // r = 1/50 + 1/70 = (70 + 50) / 3500 = 120 / 3500 = 12 / 350
    double expected_hazard_rate = 1.0 / 50.0 + 1.0 / 70.0;
    double expected_crit_time = -std::log(0.90) / expected_hazard_rate;

    ASSERT_NEAR(broker.hazard_rate(0), expected_hazard_rate, 1e-9);
    ASSERT_NEAR(broker.critical_time_us(0), expected_crit_time, 1e-9);

    // Advance qubit 0 by 350.0 ns (0.35 us)
    broker.on_gate_executed(0, 350.0, 1);

    double expected_slack = expected_crit_time - (350.0 / 1000.0);
    ASSERT_NEAR(broker.slack_us(0), expected_slack, 1e-9);
    ASSERT_NEAR(broker.elapsed_ns(0), 350.0, 1e-9);

    // Test load from data/telemetry.conf
    TelemetryConfig loaded = TelemetryConfig::load_from_file("data/telemetry.conf");
    ASSERT_NEAR(loaded.t1_us, 50.0, 1e-9);
    ASSERT_NEAR(loaded.t2_us, 70.0, 1e-9);
    ASSERT_NEAR(loaded.fidelity_threshold, 0.90, 1e-9);
    ASSERT_NEAR(loaded.alert_slack_us, 5.0, 1e-9);
    ASSERT_NEAR(loaded.single_qubit_ns, 35.0, 1e-9);
    ASSERT_NEAR(loaded.two_qubit_ns, 300.0, 1e-9);
    ASSERT_NEAR(loaded.measure_ns, 800.0, 1e-9);
}

// T5.12: INV-1 static gate: zero FibonacciHeap references in unit2_graph
TEST(Unit5Heap, T5_12_INV1_StaticGate) {
    std::vector<std::string> unit2_files = {
        "include/qroute/unit2_graph/physical_coupling_graph.hpp",
        "include/qroute/unit2_graph/placement_map.hpp",
        "include/qroute/unit2_graph/swap_synthesizer.hpp",
        "src/unit2_graph/physical_coupling_graph.cpp",
        "src/unit2_graph/placement_map.cpp",
        "src/unit2_graph/swap_synthesizer.cpp"
    };

    for (const auto& file : unit2_files) {
        std::string content = read_text_file(file);
        ASSERT_EQ(content.find("FibonacciHeap"), std::string::npos);
        ASSERT_EQ(content.find("BinaryHeapBaseline"), std::string::npos);
    }
}

// T5.13: Deterministic FIFO tie-break for equal slack keys
TEST(Unit5Heap, T5_13_FIFO_TieBreak_EqualSlack) {
    TelemetryConfig cfg;
    cfg.t1_us = 50.0;
    cfg.t2_us = 70.0;
    cfg.fidelity_threshold = 0.90;
    cfg.alert_slack_us = 100.0; // high threshold to alert all immediately

    TelemetryBroker broker(5, cfg);

    // Execute identical gates on qubits 0, 1, 2, 3, 4 in order
    // They will all have identical slack, but sequential insertion_seq
    for (PhysicalQubit q = 0; q < 5; ++q) {
        broker.on_gate_executed(q, 100.0, q + 1);
    }

    // Extract all 5 alerts
    std::vector<PhysicalQubit> extraction_order;
    while (broker.has_critical_alert()) {
        Alert a = broker.extract_most_critical();
        extraction_order.push_back(a.qubit);
    }

    ASSERT_EQ(extraction_order.size(), 5ULL);
    // Strict FIFO order: 0, 1, 2, 3, 4
    for (PhysicalQubit q = 0; q < 5; ++q) {
        ASSERT_EQ(extraction_order[q], q);
    }
}

int main(int argc, char** argv) {
    return qroute::test::run_all_tests(argc, argv);
}
