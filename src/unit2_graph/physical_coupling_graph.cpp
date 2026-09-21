#include "qroute/unit2_graph/physical_coupling_graph.hpp"
#include "qroute/common/file_io.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace qroute::unit2 {

PhysicalCouplingGraph::PhysicalCouplingGraph(uint32_t vertex_count)
    : n_(vertex_count), edge_count_(0), adj_(vertex_count) {}

void PhysicalCouplingGraph::add_undirected_edge(uint32_t u, uint32_t v, double fidelity, double gate_time_ns) {
    if (u >= n_ || v >= n_) {
        throw std::out_of_range("Vertex index out of range in add_undirected_edge");
    }
    if (u == v) {
        throw std::invalid_argument("Self-loop edge rejected: " + std::to_string(u) + " == " + std::to_string(v));
    }
    if (fidelity <= 0.0 || fidelity > 1.0 || std::isnan(fidelity)) {
        throw std::invalid_argument("fidelity must be in (0, 1]");
    }
    if (gate_time_ns < 0.0) {
        throw std::invalid_argument("gate_time_ns must be >= 0");
    }
    for (const auto& e : adj_[u]) {
        if (e.to == v) {
            throw std::invalid_argument("Duplicate edge rejected: (" + std::to_string(u) + ", " + std::to_string(v) + ")");
        }
    }

    double weight = -std::log(fidelity);
    if (weight < 0.0) {
        weight = 0.0;
    }

    uint32_t rev_u = static_cast<uint32_t>(adj_[v].size());
    uint32_t rev_v = static_cast<uint32_t>(adj_[u].size());

    adj_[u].push_back(PhysicalEdge{v, rev_u, fidelity, weight, gate_time_ns});
    adj_[v].push_back(PhysicalEdge{u, rev_v, fidelity, weight, gate_time_ns});
    edge_count_++;
    apsp_computed_ = false;
}

void PhysicalCouplingGraph::sort_adjacency() {
    for (uint32_t u = 0; u < n_; ++u) {
        std::sort(adj_[u].begin(), adj_[u].end(), [](const PhysicalEdge& a, const PhysicalEdge& b) {
            return a.to < b.to;
        });
    }

    for (uint32_t u = 0; u < n_; ++u) {
        for (size_t idx = 0; idx < adj_[u].size(); ++idx) {
            uint32_t v = adj_[u][idx].to;
            for (size_t rev = 0; rev < adj_[v].size(); ++rev) {
                if (adj_[v][rev].to == u) {
                    adj_[u][idx].reverse_index = static_cast<uint32_t>(rev);
                    break;
                }
            }
        }
    }
    apsp_computed_ = false;
}

PhysicalCouplingGraph PhysicalCouplingGraph::load_from_file(const std::string& path) {
    std::vector<std::string> lines = read_lines(path);
    struct LineRecord {
        size_t line_num;
        std::string content;
    };
    std::vector<LineRecord> valid_lines;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string stripped = strip_comment(lines[i]);
        size_t first = stripped.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = stripped.find_last_not_of(" \t\r\n");
        valid_lines.push_back({i + 1, stripped.substr(first, last - first + 1)});
    }

    if (valid_lines.empty()) {
        throw std::runtime_error("Empty or comment-only coupling map file: " + path);
    }

    auto header_tokens = split_tokens(valid_lines[0].content);
    if (header_tokens.size() < 2) {
        throw std::runtime_error("Malformed header line " + std::to_string(valid_lines[0].line_num) + " in " + path);
    }

    uint32_t v_count = 0;
    size_t e_count = 0;
    try {
        v_count = static_cast<uint32_t>(std::stoul(header_tokens[0]));
        e_count = static_cast<size_t>(std::stoull(header_tokens[1]));
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse header on line " + std::to_string(valid_lines[0].line_num) + ": " + e.what());
    }

    if (valid_lines.size() - 1 != e_count) {
        throw std::runtime_error("Coupling map edge count mismatch in " + path + ": expected " +
                                 std::to_string(e_count) + ", found " + std::to_string(valid_lines.size() - 1));
    }

    PhysicalCouplingGraph graph(v_count);
    for (size_t i = 1; i < valid_lines.size(); ++i) {
        auto tokens = split_tokens(valid_lines[i].content);
        if (tokens.size() < 4) {
            throw std::runtime_error("Malformed edge specification on line " + std::to_string(valid_lines[i].line_num) + " in " + path);
        }
        uint32_t u = static_cast<uint32_t>(std::stoul(tokens[0]));
        uint32_t v = static_cast<uint32_t>(std::stoul(tokens[1]));
        double fidelity = std::stod(tokens[2]);
        double gate_time = std::stod(tokens[3]);
        graph.add_undirected_edge(u, v, fidelity, gate_time);
    }

    graph.sort_adjacency();
    return graph;
}

const std::vector<PhysicalEdge>& PhysicalCouplingGraph::neighbors(uint32_t u) const {
    if (u >= n_) {
        throw std::out_of_range("Vertex index out of range in neighbors");
    }
    return adj_[u];
}

bool PhysicalCouplingGraph::has_edge(uint32_t u, uint32_t v) const noexcept {
    if (u >= n_ || v >= n_) return false;
    for (const auto& e : adj_[u]) {
        if (e.to == v) return true;
    }
    return false;
}

double PhysicalCouplingGraph::edge_fidelity(uint32_t u, uint32_t v) const {
    if (u >= n_ || v >= n_) {
        throw std::out_of_range("Vertex index out of range in edge_fidelity");
    }
    for (const auto& e : adj_[u]) {
        if (e.to == v) return e.fidelity;
    }
    throw std::invalid_argument("No edge exists between " + std::to_string(u) + " and " + std::to_string(v));
}

double PhysicalCouplingGraph::edge_weight(uint32_t u, uint32_t v) const {
    if (u >= n_ || v >= n_) {
        throw std::out_of_range("Vertex index out of range in edge_weight");
    }
    for (const auto& e : adj_[u]) {
        if (e.to == v) return e.weight;
    }
    throw std::invalid_argument("No edge exists between " + std::to_string(u) + " and " + std::to_string(v));
}

double PhysicalCouplingGraph::edge_gate_time_ns(uint32_t u, uint32_t v) const {
    if (u >= n_ || v >= n_) {
        throw std::out_of_range("Vertex index out of range in edge_gate_time_ns");
    }
    for (const auto& e : adj_[u]) {
        if (e.to == v) return e.gate_time_ns;
    }
    throw std::invalid_argument("No edge exists between " + std::to_string(u) + " and " + std::to_string(v));
}

std::vector<uint32_t> PhysicalCouplingGraph::bfs_order(uint32_t source) const {
    if (source >= n_) return {};
    std::vector<uint32_t> order;
    order.reserve(n_);
    std::vector<bool> visited(n_, false);
    std::queue<uint32_t> q;

    visited[source] = true;
    q.push(source);

    while (!q.empty()) {
        uint32_t u = q.front();
        q.pop();
        order.push_back(u);

        for (const auto& e : adj_[u]) {
            if (!visited[e.to]) {
                visited[e.to] = true;
                q.push(e.to);
            }
        }
    }
    return order;
}

std::vector<uint32_t> PhysicalCouplingGraph::dfs_order(uint32_t source) const {
    if (source >= n_) return {};
    std::vector<uint32_t> order;
    order.reserve(n_);
    std::vector<bool> visited(n_, false);
    std::vector<uint32_t> stack;
    stack.reserve(n_);

    stack.push_back(source);

    while (!stack.empty()) {
        uint32_t u = stack.back();
        stack.pop_back();

        if (visited[u]) continue;
        visited[u] = true;
        order.push_back(u);

        for (auto it = adj_[u].rbegin(); it != adj_[u].rend(); ++it) {
            if (!visited[it->to]) {
                stack.push_back(it->to);
            }
        }
    }
    return order;
}

bool PhysicalCouplingGraph::is_connected() const noexcept {
    if (n_ <= 1) return true;
    return bfs_order(0).size() == n_;
}

std::vector<uint32_t> PhysicalCouplingGraph::connected_component(uint32_t source) const {
    return bfs_order(source);
}

uint32_t PhysicalCouplingGraph::connected_component_count() const {
    if (n_ == 0) return 0;
    std::vector<bool> visited(n_, false);
    uint32_t count = 0;

    for (uint32_t i = 0; i < n_; ++i) {
        if (!visited[i]) {
            count++;
            std::queue<uint32_t> q;
            visited[i] = true;
            q.push(i);

            while (!q.empty()) {
                uint32_t u = q.front();
                q.pop();
                for (const auto& e : adj_[u]) {
                    if (!visited[e.to]) {
                        visited[e.to] = true;
                        q.push(e.to);
                    }
                }
            }
        }
    }
    return count;
}

std::vector<double> PhysicalCouplingGraph::dijkstra(
    uint32_t source, std::vector<uint32_t>* prev) const
{
    std::vector<double> dist(n_, kInfinity);
    if (prev) prev->assign(n_, kNil);
    if (source >= n_) return dist;

    dist[source] = 0.0;

    // [INV-1] binary heap ONLY. Deterministic tie-break: (cost ASC, vertex ASC).
    std::priority_queue<
        std::pair<double, uint32_t>,
        std::vector<std::pair<double, uint32_t>>,
        std::greater<std::pair<double, uint32_t>>
    > pq;

    pq.push({0.0, source});

    while (!pq.empty()) {
        const auto [d, u] = pq.top();
        pq.pop();
        if (d > dist[u] + kEpsilon) continue; // stale entry (lazy deletion)

        for (const PhysicalEdge& e : adj_[u]) {
            const double nd = d + e.weight;
            if (nd + kEpsilon < dist[e.to]) {
                dist[e.to] = nd;
                if (prev) (*prev)[e.to] = u;
                pq.push({nd, e.to});
            }
        }
    }
    return dist;
}

std::vector<uint32_t> PhysicalCouplingGraph::shortest_path(uint32_t source, uint32_t target) const {
    if (source >= n_ || target >= n_) return {};
    if (source == target) return {source};

    std::vector<uint32_t> prev;
    std::vector<double> dist = dijkstra(source, &prev);

    if (dist[target] >= kInfinity || prev[target] == kNil) {
        return {};
    }

    std::vector<uint32_t> path;
    uint32_t curr = target;
    while (curr != kNil) {
        path.push_back(curr);
        if (curr == source) break;
        curr = prev[curr];
    }

    if (path.empty() || path.back() != source) {
        return {};
    }

    std::reverse(path.begin(), path.end());

    // Invariant assertion: consecutive edge adjacency
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        assert(has_edge(path[i], path[i + 1]));
    }

    return path;
}

void PhysicalCouplingGraph::compute_all_pairs() {
    hop_matrix_.assign(static_cast<size_t>(n_) * n_, std::numeric_limits<uint16_t>::max());
    cost_matrix_.assign(static_cast<size_t>(n_) * n_, kInfinity);

    for (uint32_t u = 0; u < n_; ++u) {
        std::vector<uint16_t> hdist(n_, std::numeric_limits<uint16_t>::max());
        std::queue<uint32_t> q;
        hdist[u] = 0;
        q.push(u);
        while (!q.empty()) {
            uint32_t curr = q.front();
            q.pop();
            for (const auto& e : adj_[curr]) {
                if (hdist[e.to] == std::numeric_limits<uint16_t>::max()) {
                    hdist[e.to] = static_cast<uint16_t>(hdist[curr] + 1);
                    q.push(e.to);
                }
            }
        }
        for (uint32_t v = 0; v < n_; ++v) {
            hop_matrix_[static_cast<size_t>(u) * n_ + v] = hdist[v];
        }

        std::vector<double> wdist = dijkstra(u);
        for (uint32_t v = 0; v < n_; ++v) {
            cost_matrix_[static_cast<size_t>(u) * n_ + v] = wdist[v];
        }
    }
    apsp_computed_ = true;
}

uint16_t PhysicalCouplingGraph::hops(uint32_t u, uint32_t v) const {
    if (!apsp_computed_) {
        const_cast<PhysicalCouplingGraph*>(this)->compute_all_pairs();
    }
    if (u >= n_ || v >= n_) return std::numeric_limits<uint16_t>::max();
    return hop_matrix_[static_cast<size_t>(u) * n_ + v];
}

double PhysicalCouplingGraph::weighted_distance(uint32_t u, uint32_t v) const {
    if (!apsp_computed_) {
        const_cast<PhysicalCouplingGraph*>(this)->compute_all_pairs();
    }
    if (u >= n_ || v >= n_) return kInfinity;
    return cost_matrix_[static_cast<size_t>(u) * n_ + v];
}

std::vector<std::tuple<uint32_t, uint32_t, double>> PhysicalCouplingGraph::mst_prim() const {
    if (n_ <= 1) return {};
    std::vector<std::tuple<uint32_t, uint32_t, double>> mst;
    std::vector<bool> in_mst(n_, false);
    using EdgeTuple = std::tuple<double, uint32_t, uint32_t>;
    std::priority_queue<EdgeTuple, std::vector<EdgeTuple>, std::greater<EdgeTuple>> pq;

    in_mst[0] = true;
    for (const auto& e : adj_[0]) {
        pq.push({e.weight, 0, e.to});
    }

    while (!pq.empty() && mst.size() < n_ - 1) {
        auto [w, u, v] = pq.top();
        pq.pop();
        if (in_mst[v]) continue;
        in_mst[v] = true;
        mst.push_back({u, v, w});
        for (const auto& e : adj_[v]) {
            if (!in_mst[e.to]) {
                pq.push({e.weight, v, e.to});
            }
        }
    }
    return mst;
}

namespace {
struct DisjointSet {
    std::vector<uint32_t> parent;
    std::vector<uint32_t> rank;
    explicit DisjointSet(uint32_t size) : parent(size), rank(size, 0) {
        for (uint32_t i = 0; i < size; ++i) parent[i] = i;
    }
    uint32_t find(uint32_t i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]);
    }
    bool unite(uint32_t i, uint32_t j) {
        uint32_t root_i = find(i);
        uint32_t root_j = find(j);
        if (root_i == root_j) return false;
        if (rank[root_i] < rank[root_j]) {
            parent[root_i] = root_j;
        } else if (rank[root_i] > rank[root_j]) {
            parent[root_j] = root_i;
        } else {
            parent[root_j] = root_i;
            rank[root_i]++;
        }
        return true;
    }
};
} // anonymous namespace

std::vector<std::tuple<uint32_t, uint32_t, double>> PhysicalCouplingGraph::mst_kruskal() const {
    if (n_ <= 1) return {};
    struct EdgeCandidate {
        uint32_t u;
        uint32_t v;
        double weight;
    };
    std::vector<EdgeCandidate> edges;
    for (uint32_t u = 0; u < n_; ++u) {
        for (const auto& e : adj_[u]) {
            if (u < e.to) {
                edges.push_back({u, e.to, e.weight});
            }
        }
    }
    std::sort(edges.begin(), edges.end(), [](const EdgeCandidate& a, const EdgeCandidate& b) {
        if (std::abs(a.weight - b.weight) > kEpsilon) {
            return a.weight < b.weight;
        }
        if (a.u != b.u) return a.u < b.u;
        return a.v < b.v;
    });

    DisjointSet dsu(n_);
    std::vector<std::tuple<uint32_t, uint32_t, double>> mst;
    for (const auto& edge : edges) {
        if (dsu.unite(edge.u, edge.v)) {
            mst.push_back({edge.u, edge.v, edge.weight});
            if (mst.size() == n_ - 1) break;
        }
    }
    return mst;
}

double PhysicalCouplingGraph::mst_total_weight(
    const std::vector<std::tuple<uint32_t, uint32_t, double>>& mst) const
{
    double total = 0.0;
    for (const auto& [u, v, w] : mst) {
        (void)u; (void)v;
        total += w;
    }
    return total;
}

std::vector<double> PhysicalCouplingGraph::bellman_ford(uint32_t source) const {
    std::vector<double> dist(n_, kInfinity);
    if (source >= n_) return dist;
    dist[source] = 0.0;

    struct DirEdge {
        uint32_t u;
        uint32_t v;
        double w;
    };
    std::vector<DirEdge> all_edges;
    for (uint32_t u = 0; u < n_; ++u) {
        for (const auto& e : adj_[u]) {
            all_edges.push_back({u, e.to, e.weight});
        }
    }

    for (uint32_t i = 1; i < n_; ++i) {
        bool any_relaxed = false;
        for (const auto& edge : all_edges) {
            if (dist[edge.u] < kInfinity) {
                if (dist[edge.u] + edge.w + kEpsilon < dist[edge.v]) {
                    dist[edge.v] = dist[edge.u] + edge.w;
                    any_relaxed = true;
                }
            }
        }
        if (!any_relaxed) break;
    }
    return dist;
}

std::vector<std::vector<double>> PhysicalCouplingGraph::floyd_warshall() const {
    std::vector<std::vector<double>> dist(n_, std::vector<double>(n_, kInfinity));
    for (uint32_t i = 0; i < n_; ++i) {
        dist[i][i] = 0.0;
        for (const auto& e : adj_[i]) {
            dist[i][e.to] = std::min(dist[i][e.to], e.weight);
        }
    }
    for (uint32_t k = 0; k < n_; ++k) {
        for (uint32_t i = 0; i < n_; ++i) {
            for (uint32_t j = 0; j < n_; ++j) {
                if (dist[i][k] < kInfinity && dist[k][j] < kInfinity) {
                    double candidate = dist[i][k] + dist[k][j];
                    if (candidate + kEpsilon < dist[i][j]) {
                        dist[i][j] = candidate;
                    }
                }
            }
        }
    }
    return dist;
}

bool PhysicalCouplingGraph::validate_against_oracles(double tol) const {
    auto fw = floyd_warshall();
    for (uint32_t u = 0; u < n_; ++u) {
        auto dij = dijkstra(u);
        auto bf = bellman_ford(u);
        for (uint32_t v = 0; v < n_; ++v) {
            if (std::isinf(dij[v]) != std::isinf(bf[v])) return false;
            if (!std::isinf(dij[v]) && std::abs(dij[v] - bf[v]) > tol) return false;

            if (std::isinf(dij[v]) != std::isinf(fw[u][v])) return false;
            if (!std::isinf(dij[v]) && std::abs(dij[v] - fw[u][v]) > tol) return false;
        }
    }
    return true;
}

} // namespace qroute::unit2
