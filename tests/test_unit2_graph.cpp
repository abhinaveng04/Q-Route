#include "native_assert.hpp"
#include "qroute/common/constants.hpp"
#include "qroute/common/file_io.hpp"
#include "qroute/common/types.hpp"
#include "qroute/unit2_graph/physical_coupling_graph.hpp"
#include "qroute/unit2_graph/physical_edge.hpp"
#include "qroute/unit2_graph/placement_map.hpp"
#include "qroute/unit2_graph/swap_synthesizer.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace qroute;
using namespace qroute::unit2;

// Memory contract check
TEST(Unit2Graph, MemoryContract_PhysicalEdge32Bytes) {
    ASSERT_EQ(sizeof(PhysicalEdge), 32ULL);
}

// T2.1: Domain validation (fidelity domain, self-loop, duplicate, OOB vertex)
TEST(Unit2Graph, T2_1_DomainValidation) {
    PhysicalCouplingGraph g(4);

    // Fidelity domain checks
    ASSERT_THROW(g.add_undirected_edge(0, 1, 1.05, 100.0), std::invalid_argument); // F > 1
    ASSERT_THROW(g.add_undirected_edge(0, 1, 0.0, 100.0), std::invalid_argument);  // F == 0
    ASSERT_THROW(g.add_undirected_edge(0, 1, -0.5, 100.0), std::invalid_argument); // F < 0
    ASSERT_THROW(g.add_undirected_edge(0, 1, 0.99, -10.0), std::invalid_argument); // gate_time < 0

    // Self-loop check
    ASSERT_THROW(g.add_undirected_edge(1, 1, 0.99, 100.0), std::invalid_argument); // u == v

    // Out of bounds check
    ASSERT_THROW(g.add_undirected_edge(0, 4, 0.99, 100.0), std::out_of_range); // v >= n
    ASSERT_THROW(g.add_undirected_edge(4, 1, 0.99, 100.0), std::out_of_range); // u >= n

    // Normal edge addition
    ASSERT_NO_THROW(g.add_undirected_edge(0, 1, 0.995, 120.0));
    ASSERT_EQ(g.edge_count(), 1ULL);

    // Duplicate edge check
    ASSERT_THROW(g.add_undirected_edge(0, 1, 0.990, 100.0), std::invalid_argument);
    ASSERT_THROW(g.add_undirected_edge(1, 0, 0.990, 100.0), std::invalid_argument);
}

// T2.2: Additive weight correctness (w = -ln(F) >= 0)
TEST(Unit2Graph, T2_2_AdditiveWeightCorrectness) {
    PhysicalCouplingGraph g(6);
    std::vector<double> fidelities = {1.0, 0.999, 0.995, 0.90, 0.50};

    g.add_undirected_edge(0, 1, fidelities[0], 100.0);
    g.add_undirected_edge(1, 2, fidelities[1], 100.0);
    g.add_undirected_edge(2, 3, fidelities[2], 100.0);
    g.add_undirected_edge(3, 4, fidelities[3], 100.0);
    g.add_undirected_edge(4, 5, fidelities[4], 100.0);

    for (size_t i = 0; i < fidelities.size(); ++i) {
        double w = g.edge_weight(static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1));
        ASSERT_GE(w, 0.0);
        double expected = -std::log(fidelities[i]);
        ASSERT_NEAR(w, expected, 1e-12);
    }
    // Perfect link F = 1.0 has w = 0.0
    ASSERT_NEAR(g.edge_weight(0, 1), 0.0, 1e-12);
}

// T2.3: is_connected() / connected_component_count() on connected and split topologies
TEST(Unit2Graph, T2_3_ConnectivityAndComponents) {
    PhysicalCouplingGraph g1(1);
    ASSERT_TRUE(g1.is_connected());
    ASSERT_EQ(g1.connected_component_count(), 1ULL);

    // Connected linear 5
    PhysicalCouplingGraph g_lin = PhysicalCouplingGraph::load_from_file("data/coupling.map");
    ASSERT_TRUE(g_lin.is_connected());
    ASSERT_EQ(g_lin.connected_component_count(), 1ULL);

    // Disconnected graph with 4 vertices
    PhysicalCouplingGraph g_split(4);
    g_split.add_undirected_edge(0, 1, 0.99, 100.0);
    g_split.add_undirected_edge(2, 3, 0.99, 100.0);
    ASSERT_FALSE(g_split.is_connected());
    ASSERT_EQ(g_split.connected_component_count(), 2ULL);

    auto comp0 = g_split.connected_component(0);
    ASSERT_EQ(comp0.size(), 2ULL);
    ASSERT_EQ(comp0[0], 0ULL);
    ASSERT_EQ(comp0[1], 1ULL);

    auto comp2 = g_split.connected_component(2);
    ASSERT_EQ(comp2.size(), 2ULL);
    ASSERT_EQ(comp2[0], 2ULL);
    ASSERT_EQ(comp2[1], 3ULL);
}

// T2.4: Dijkstra matches Floyd-Warshall on 3 topologies
TEST(Unit2Graph, T2_4_DijkstraMatchesFloydWarshall) {
    std::vector<std::string> topologies = {
        "data/coupling.map",
        "data/coupling_grid9.map",
        "data/coupling_heavyhex7.map"
    };

    for (const auto& path : topologies) {
        PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file(path);
        ASSERT_TRUE(g.is_connected());
        auto fw = g.floyd_warshall();
        for (uint32_t u = 0; u < g.vertex_count(); ++u) {
            auto dij = g.dijkstra(u);
            for (uint32_t v = 0; v < g.vertex_count(); ++v) {
                ASSERT_NEAR(dij[v], fw[u][v], 1e-9);
            }
        }
    }
}

// T2.5: Dijkstra matches Bellman-Ford on all 3 topologies
TEST(Unit2Graph, T2_5_DijkstraMatchesBellmanFord) {
    std::vector<std::string> topologies = {
        "data/coupling.map",
        "data/coupling_grid9.map",
        "data/coupling_heavyhex7.map"
    };

    for (const auto& path : topologies) {
        PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file(path);
        for (uint32_t u = 0; u < g.vertex_count(); ++u) {
            auto dij = g.dijkstra(u);
            auto bf = g.bellman_ford(u);
            for (uint32_t v = 0; v < g.vertex_count(); ++v) {
                ASSERT_NEAR(dij[v], bf[v], 1e-9);
            }
        }
        ASSERT_TRUE(g.validate_against_oracles(1e-9));
    }
}

// T2.6: Shortest path consecutive adjacency and endpoint validation
TEST(Unit2Graph, T2_6_ShortestPathValidity) {
    PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file("data/coupling.map");

    auto path = g.shortest_path(0, 4);
    ASSERT_EQ(path.size(), 5ULL);
    ASSERT_EQ(path.front(), 0ULL);
    ASSERT_EQ(path.back(), 4ULL);

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        ASSERT_TRUE(g.has_edge(path[i], path[i + 1]));
    }

    // Same vertex path
    auto same = g.shortest_path(2, 2);
    ASSERT_EQ(same.size(), 1ULL);
    ASSERT_EQ(same[0], 2ULL);

    // Unreachable path in split graph
    PhysicalCouplingGraph g_split(4);
    g_split.add_undirected_edge(0, 1, 0.99, 100.0);
    g_split.add_undirected_edge(2, 3, 0.99, 100.0);
    auto unreach = g_split.shortest_path(0, 3);
    ASSERT_TRUE(unreach.empty());
}

// T2.7: BFS hop matrix == analytic |i - j| on linear topology
TEST(Unit2Graph, T2_7_BfsHopMatrixAnalytic) {
    PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file("data/coupling.map");
    g.compute_all_pairs();

    for (uint32_t i = 0; i < 5; ++i) {
        for (uint32_t j = 0; j < 5; ++j) {
            uint16_t expected_hops = static_cast<uint16_t>(std::abs(static_cast<int>(i) - static_cast<int>(j)));
            ASSERT_EQ(g.hops(i, j), expected_hops);
        }
    }
}

// T2.8: Prim and Kruskal total weights match
TEST(Unit2Graph, T2_8_PrimAndKruskalWeightsMatch) {
    std::vector<std::string> topologies = {
        "data/coupling.map",
        "data/coupling_grid9.map",
        "data/coupling_heavyhex7.map"
    };

    for (const auto& path : topologies) {
        PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file(path);
        auto mst_p = g.mst_prim();
        auto mst_k = g.mst_kruskal();

        ASSERT_EQ(mst_p.size(), static_cast<size_t>(g.vertex_count() - 1));
        ASSERT_EQ(mst_k.size(), static_cast<size_t>(g.vertex_count() - 1));

        double wt_p = g.mst_total_weight(mst_p);
        double wt_k = g.mst_total_weight(mst_k);

        ASSERT_NEAR(wt_p, wt_k, 1e-9);
    }
}

// T2.9: Determinism: 20 identical Dijkstra runs produce byte-identical dist and prev arrays
TEST(Unit2Graph, T2_9_DijkstraDeterminism20Runs) {
    PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file("data/coupling_grid9.map");

    std::vector<uint32_t> base_prev;
    std::vector<double> base_dist = g.dijkstra(0, &base_prev);

    for (int run = 0; run < 19; ++run) {
        std::vector<uint32_t> cur_prev;
        std::vector<double> cur_dist = g.dijkstra(0, &cur_prev);

        ASSERT_EQ(cur_dist.size(), base_dist.size());
        ASSERT_EQ(cur_prev.size(), base_prev.size());

        int dist_cmp = std::memcmp(base_dist.data(), cur_dist.data(), sizeof(double) * base_dist.size());
        ASSERT_EQ(dist_cmp, 0);

        int prev_cmp = std::memcmp(base_prev.data(), cur_prev.data(), sizeof(uint32_t) * base_prev.size());
        ASSERT_EQ(prev_cmp, 0);
    }
}

// T2.10: SwapSynthesizer legality across a 20-gate trace on grid_3x3
TEST(Unit2Graph, T2_10_SwapSynthesizerLegalityTrace) {
    PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file("data/coupling_grid9.map");
    PlacementMap map(9, 9);
    for (uint32_t i = 0; i < 9; ++i) {
        map.set(i, i);
    }

    SwapSynthesizer synth;
    std::vector<std::pair<LogicalQubit, LogicalQubit>> trace = {
        {0, 8}, {1, 7}, {2, 6}, {3, 5}, {0, 2},
        {6, 8}, {1, 5}, {3, 7}, {0, 4}, {4, 8},
        {2, 4}, {4, 6}, {1, 3}, {5, 7}, {0, 7},
        {1, 8}, {2, 3}, {5, 6}, {0, 5}, {3, 8}
    };

    ASSERT_EQ(trace.size(), 20ULL);

    for (const auto& [a, b] : trace) {
        RoutingPlan plan = synth.route(g, map, a, b);
        ASSERT_FALSE(plan.gates.empty());
        for (const auto& gate : plan.gates) {
            ASSERT_TRUE(g.has_edge(gate.control, gate.target));
        }
    }
}

// T2.11: PlacementMap bijection invariant maintained across full trace
TEST(Unit2Graph, T2_11_PlacementMapBijectionInvariant) {
    PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file("data/coupling_grid9.map");
    PlacementMap map(9, 9);
    for (uint32_t i = 0; i < 9; ++i) {
        map.set(i, i);
    }
    ASSERT_TRUE(map.validate_bijection());

    SwapSynthesizer synth;
    std::vector<std::pair<LogicalQubit, LogicalQubit>> trace = {
        {0, 8}, {1, 7}, {2, 6}, {3, 5}, {0, 2},
        {6, 8}, {1, 5}, {3, 7}, {0, 4}, {4, 8},
        {2, 4}, {4, 6}, {1, 3}, {5, 7}, {0, 7},
        {1, 8}, {2, 3}, {5, 6}, {0, 5}, {3, 8}
    };

    for (const auto& [a, b] : trace) {
        synth.route(g, map, a, b);
        ASSERT_TRUE(map.validate_bijection());
    }

    // Direct swap_physical test
    map.swap_physical(0, 1);
    ASSERT_TRUE(map.validate_bijection());
    map.swap_physical(2, 5);
    ASSERT_TRUE(map.validate_bijection());
    map.swap_physical(3, 8);
    ASSERT_TRUE(map.validate_bijection());
}

// T2.12: SWAP accounting identity added_cnot == 3 * swap_count
TEST(Unit2Graph, T2_12_SwapAccountingIdentity) {
    PhysicalCouplingGraph g = PhysicalCouplingGraph::load_from_file("data/coupling_grid9.map");
    PlacementMap map(9, 9);
    for (uint32_t i = 0; i < 9; ++i) {
        map.set(i, i);
    }

    SwapSynthesizer synth;
    std::vector<std::pair<LogicalQubit, LogicalQubit>> trace = {
        {0, 1}, // adjacent: 0 swaps
        {0, 8}, // 4 hops: 3 swaps
        {2, 6}, // multi-hop
        {1, 2}, // test adjacent again
        {3, 7}  // multi-hop
    };

    for (const auto& [a, b] : trace) {
        RoutingPlan plan = synth.route(g, map, a, b);
        ASSERT_EQ(plan.added_cnot_count, 3 * plan.swap_count);
        ASSERT_EQ(plan.gates.size(), static_cast<size_t>(1 + plan.added_cnot_count));
        ASSERT_NEAR(plan.path_fidelity, std::exp(-plan.path_weight), 1e-12);
    }

    // decompose_swap unit test
    auto decomp = SwapSynthesizer::decompose_swap(g, 0, 1);
    ASSERT_EQ(decomp.size(), 3ULL);
    ASSERT_TRUE(decomp[0].is_synthetic_swap_component);
    ASSERT_TRUE(decomp[1].is_synthetic_swap_component);
    ASSERT_TRUE(decomp[2].is_synthetic_swap_component);
    ASSERT_EQ(decomp[0].control, 0ULL); ASSERT_EQ(decomp[0].target, 1ULL);
    ASSERT_EQ(decomp[1].control, 1ULL); ASSERT_EQ(decomp[1].target, 0ULL);
    ASSERT_EQ(decomp[2].control, 0ULL); ASSERT_EQ(decomp[2].target, 1ULL);
}

// T2.13: INV-1 static gate: zero FibonacciHeap in unit2 sources
TEST(Unit2Graph, T2_13_INV1_StaticGate) {
    std::vector<std::string> files = {
        "include/qroute/unit2_graph/physical_edge.hpp",
        "include/qroute/unit2_graph/physical_coupling_graph.hpp",
        "include/qroute/unit2_graph/placement_map.hpp",
        "include/qroute/unit2_graph/swap_synthesizer.hpp",
        "src/unit2_graph/physical_coupling_graph.cpp",
        "src/unit2_graph/placement_map.cpp",
        "src/unit2_graph/swap_synthesizer.cpp"
    };

    for (const auto& file : files) {
        std::string content = read_text_file(file);
        // Ensure the forbidden substring does not appear anywhere
        std::string forbidden = "Fibonacci";
        forbidden += "Heap";
        ASSERT_EQ(content.find(forbidden), std::string::npos);
    }
}

int main(int argc, char** argv) {
    return qroute::test::run_all_tests(argc, argv);
}
