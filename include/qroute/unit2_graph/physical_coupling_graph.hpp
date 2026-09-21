#pragma once

#include "qroute/common/constants.hpp"
#include "qroute/common/types.hpp"
#include "qroute/unit2_graph/physical_edge.hpp"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace qroute::unit2 {

class PhysicalCouplingGraph {
public:
    explicit PhysicalCouplingGraph(uint32_t vertex_count);

    // Topology construction (validates fidelity domain, rejects self-loops/duplicates)
    void add_undirected_edge(uint32_t u, uint32_t v, double fidelity, double gate_time_ns);
    void addundirectededge(uint32_t u, uint32_t v, double fidelity, double gate_time_ns) {
        add_undirected_edge(u, v, fidelity, gate_time_ns);
    }

    static PhysicalCouplingGraph load_from_file(const std::string& path);
    static PhysicalCouplingGraph loadfromfile(const std::string& path) {
        return load_from_file(path);
    }

    void sort_adjacency(); // sort each adj_[u] by to ascending -- MANDATORY for determinism
    void sortadjacency() { sort_adjacency(); }

    uint32_t vertex_count() const noexcept { return n_; }
    uint32_t vertexcount() const noexcept { return vertex_count(); }

    size_t edge_count() const noexcept { return edge_count_; }
    size_t edgecount() const noexcept { return edge_count(); }

    const std::vector<PhysicalEdge>& neighbors(uint32_t u) const;
    bool has_edge(uint32_t u, uint32_t v) const noexcept;
    bool hasedge(uint32_t u, uint32_t v) const noexcept { return has_edge(u, v); }

    double edge_fidelity(uint32_t u, uint32_t v) const;
    double edgefidelity(uint32_t u, uint32_t v) const { return edge_fidelity(u, v); }

    double edge_weight(uint32_t u, uint32_t v) const;
    double edgeweight(uint32_t u, uint32_t v) const { return edge_weight(u, v); }

    double edge_gate_time_ns(uint32_t u, uint32_t v) const;
    double edge_gate_time(uint32_t u, uint32_t v) const { return edge_gate_time_ns(u, v); }

    // ---- Traversals / connectivity ----
    bool is_connected() const noexcept;
    bool isconnected() const noexcept { return is_connected(); }

    std::vector<uint32_t> bfs_order(uint32_t source) const;
    std::vector<uint32_t> bfsorder(uint32_t source) const { return bfs_order(source); }

    std::vector<uint32_t> dfs_order(uint32_t source) const; // iterative, explicit stack
    std::vector<uint32_t> dfsorder(uint32_t source) const { return dfs_order(source); }

    std::vector<uint32_t> connected_component(uint32_t source) const;
    std::vector<uint32_t> connectedcomponent(uint32_t source) const { return connected_component(source); }

    uint32_t connected_component_count() const;
    uint32_t connectedcomponent_count() const { return connected_component_count(); }

    // ---- Shortest paths: [INV-1] std::priority_queue ONLY ----
    std::vector<double> dijkstra(uint32_t source, std::vector<uint32_t>* prev = nullptr) const;
    std::vector<uint32_t> shortest_path(uint32_t source, uint32_t target) const; // empty if unreachable
    std::vector<uint32_t> shortestpath(uint32_t source, uint32_t target) const { return shortest_path(source, target); }

    // ---- All-pairs cache used by Unit 4 lower bounds ----
    void compute_all_pairs(); // BFS hops + Dijkstra weights
    void computeallpairs() { compute_all_pairs(); }

    uint16_t hops(uint32_t u, uint32_t v) const; // UINT16_MAX if unreachable
    double weighted_distance(uint32_t u, uint32_t v) const;
    double weighteddistance(uint32_t u, uint32_t v) const { return weighted_distance(u, v); }

    // ---- Spanning trees (syllabus completeness) ----
    std::vector<std::tuple<uint32_t, uint32_t, double>> mst_prim() const;
    std::vector<std::tuple<uint32_t, uint32_t, double>> mst_kruskal() const;
    double mst_total_weight(const std::vector<std::tuple<uint32_t, uint32_t, double>>& mst) const;
    double msttotalweight(const std::vector<std::tuple<uint32_t, uint32_t, double>>& mst) const {
        return mst_total_weight(mst);
    }

    // ---- Validation oracles (TEST/BENCH ONLY, never used in routing hot path) ----
    std::vector<double> bellman_ford(uint32_t source) const;
    std::vector<double> bellmanford(uint32_t source) const { return bellman_ford(source); }

    std::vector<std::vector<double>> floyd_warshall() const;
    std::vector<std::vector<double>> floydwarshall() const { return floyd_warshall(); }

    bool validate_against_oracles(double tol = 1e-9) const;
    bool validateagainstoracles(double tol = 1e-9) const { return validate_against_oracles(tol); }

private:
    uint32_t n_ = 0;
    size_t edge_count_ = 0;
    std::vector<std::vector<PhysicalEdge>> adj_;

    // Row-major dense caches: index = u * n_ + v
    mutable std::vector<uint16_t> hop_matrix_;
    mutable std::vector<double> cost_matrix_;
    mutable bool apsp_computed_ = false;
};

} // namespace qroute::unit2
