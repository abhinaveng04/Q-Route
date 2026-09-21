#include "qroute/unit1_ast/threaded_ast.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace qroute::unit1 {

void ThreadedAST::reserve(size_t node_count) {
    nodes_.reserve(node_count);
    payloads_.reserve(node_count);
}

NodeIndex ThreadedAST::append(OpCode op, GatePayload payload, uint32_t source_line) {
    const auto node_idx = static_cast<NodeIndex>(nodes_.size());
    const auto payload_idx = static_cast<NodeIndex>(payloads_.size());

    payload.source_line = source_line;
    payloads_.push_back(payload);

    ASTNode n;
    n.left = kNil;
    n.right = kNil;
    n.payload = payload_idx;
    n.seq = node_idx;
    n.source_line = source_line;
    n.op = op;
    n.left_thread = 0;
    n.right_thread = 0;
    n.reserved = 0;

    nodes_.push_back(n);
    return node_idx;
}

NodeIndex ThreadedAST::build_balanced(NodeIndex begin, NodeIndex end) {
    if (begin >= end) {
        return kNil;
    }
    NodeIndex mid = begin + (end - begin) / 2;
    nodes_[mid].left = build_balanced(begin, mid);
    nodes_[mid].right = build_balanced(mid + 1, end);
    nodes_[mid].left_thread = 0;
    nodes_[mid].right_thread = 0;
    return mid;
}

void ThreadedAST::build_from_sequence() {
    if (nodes_.empty()) {
        root_ = kNil;
        return;
    }
    root_ = build_balanced(0, static_cast<NodeIndex>(nodes_.size()));
}

void ThreadedAST::finalize_threads() {
    if (nodes_.empty()) {
        root_ = kNil;
        return;
    }

    std::vector<NodeIndex> inorder_seq;
    inorder_seq.reserve(nodes_.size());

    std::vector<NodeIndex> st;
    st.reserve(32);
    NodeIndex curr = root_;
    while (curr != kNil || !st.empty()) {
        while (curr != kNil) {
            st.push_back(curr);
            curr = (nodes_[curr].left_thread == 0) ? nodes_[curr].left : kNil;
        }
        curr = st.back();
        st.pop_back();
        inorder_seq.push_back(curr);
        curr = (nodes_[curr].right_thread == 0) ? nodes_[curr].right : kNil;
    }

    const size_t n = inorder_seq.size();
    for (size_t k = 0; k < n; ++k) {
        NodeIndex u = inorder_seq[k];
        if (nodes_[u].left == kNil) {
            nodes_[u].left_thread = 1;
            nodes_[u].left = (k > 0) ? inorder_seq[k - 1] : kNil;
        } else {
            nodes_[u].left_thread = 0;
        }

        if (nodes_[u].right == kNil) {
            nodes_[u].right_thread = 1;
            nodes_[u].right = (k + 1 < n) ? inorder_seq[k + 1] : kNil;
        } else {
            nodes_[u].right_thread = 0;
        }
    }

    // Immediately release temporary buffers
    std::vector<NodeIndex>().swap(inorder_seq);
    std::vector<NodeIndex>().swap(st);
}

const ASTNode& ThreadedAST::node(NodeIndex i) const {
    if (i >= nodes_.size()) {
        throw std::out_of_range("ASTNode index out of range: " + std::to_string(i));
    }
    return nodes_[i];
}

const GatePayload& ThreadedAST::payload(NodeIndex i) const {
    if (i >= payloads_.size()) {
        throw std::out_of_range("GatePayload index out of range: " + std::to_string(i));
    }
    return payloads_[i];
}

NodeIndex ThreadedAST::leftmost(NodeIndex start) const noexcept {
    if (start == kNil) return kNil;
    NodeIndex cur = start;
    while (cur != kNil && nodes_[cur].left != kNil && nodes_[cur].left_thread == 0) {
        cur = nodes_[cur].left;
    }
    return cur;
}

NodeIndex ThreadedAST::next(NodeIndex i) const noexcept {
    if (i == kNil) return kNil;
    if (nodes_[i].right_thread == 1) {
        return nodes_[i].right;
    }
    if (nodes_[i].right != kNil) {
        return leftmost(nodes_[i].right);
    }
    return kNil;
}

NodeIndex ThreadedAST::prev(NodeIndex i) const noexcept {
    if (i == kNil) return kNil;
    if (nodes_[i].left_thread == 1) {
        return nodes_[i].left;
    }
    if (nodes_[i].left != kNil) {
        NodeIndex cur = nodes_[i].left;
        while (cur != kNil && nodes_[cur].right != kNil && nodes_[cur].right_thread == 0) {
            cur = nodes_[cur].right;
        }
        return cur;
    }
    return kNil;
}

std::vector<NodeIndex> ThreadedAST::inorder_reference() const {
    std::vector<NodeIndex> res;
    res.reserve(nodes_.size());
    std::vector<NodeIndex> st;
    NodeIndex curr = root_;
    while (curr != kNil || !st.empty()) {
        while (curr != kNil) {
            st.push_back(curr);
            curr = (nodes_[curr].left_thread == 0) ? nodes_[curr].left : kNil;
        }
        curr = st.back();
        st.pop_back();
        res.push_back(curr);
        curr = (nodes_[curr].right_thread == 0) ? nodes_[curr].right : kNil;
    }
    return res;
}

bool ThreadedAST::validate_threads() const noexcept {
    if (nodes_.empty()) {
        return root_ == kNil;
    }
    if (root_ >= nodes_.size()) {
        return false;
    }

    // Traverse iteratively via threads
    std::vector<bool> visited(nodes_.size(), false);
    NodeIndex cur = leftmost(root_);
    NodeIndex prev_node = kNil;
    size_t count = 0;
    uint32_t expected_seq = 0;

    while (cur != kNil) {
        if (cur >= nodes_.size()) return false;
        if (visited[cur]) return false; // Cycle detection
        visited[cur] = true;

        // TH-6: Traversal order equals ascending seq
        if (nodes_[cur].seq != expected_seq) return false;
        ++expected_seq;

        // TH-2: left_thread == 1 => left is predecessor
        if (nodes_[cur].left_thread == 1) {
            if (nodes_[cur].left != prev_node) return false;
        }

        // TH-4: if previous node had right_thread == 1, its right must be cur
        if (prev_node != kNil && nodes_[prev_node].right_thread == 1) {
            if (nodes_[prev_node].right != cur) return false;
        }

        prev_node = cur;
        cur = next(cur);
        ++count;
    }

    // TH-5: Visits every node exactly once
    if (count != nodes_.size()) return false;

    // TH-4: Last node's right thread must be kNil
    if (prev_node != kNil && nodes_[prev_node].right_thread == 1) {
        if (nodes_[prev_node].right != kNil) return false;
    }

    // TH-7: Tree height satisfies height <= floor(log2(n)) + 1
    uint32_t h = height();
    uint32_t max_h = static_cast<uint32_t>(std::floor(std::log2(nodes_.size()))) + 1;
    if (h > max_h) return false;

    return true;
}

uint32_t ThreadedAST::height() const noexcept {
    if (nodes_.empty() || root_ == kNil) return 0;
    // Iterative post-order height computation using non-threaded structural links
    std::vector<std::pair<NodeIndex, uint32_t>> st;
    st.reserve(64);
    st.push_back({root_, 1});
    uint32_t max_h = 0;
    while (!st.empty()) {
        auto [idx, depth] = st.back();
        st.pop_back();
        max_h = std::max(max_h, depth);
        if (nodes_[idx].left != kNil && nodes_[idx].left_thread == 0) {
            st.push_back({nodes_[idx].left, depth + 1});
        }
        if (nodes_[idx].right != kNil && nodes_[idx].right_thread == 0) {
            st.push_back({nodes_[idx].right, depth + 1});
        }
    }
    return max_h;
}

bool ThreadedAST::is_balanced() const noexcept {
    if (nodes_.empty() || root_ == kNil) return true;

    // Compute height of each node in structural tree
    std::vector<uint32_t> node_height(nodes_.size(), 0);
    // Post-order traversal to compute heights bottom-up
    std::vector<NodeIndex> order;
    order.reserve(nodes_.size());
    std::vector<NodeIndex> st;
    st.push_back(root_);
    while (!st.empty()) {
        NodeIndex u = st.back();
        st.pop_back();
        order.push_back(u);
        if (nodes_[u].left != kNil && nodes_[u].left_thread == 0) {
            st.push_back(nodes_[u].left);
        }
        if (nodes_[u].right != kNil && nodes_[u].right_thread == 0) {
            st.push_back(nodes_[u].right);
        }
    }

    std::reverse(order.begin(), order.end());
    for (NodeIndex u : order) {
        uint32_t hl = (nodes_[u].left != kNil && nodes_[u].left_thread == 0) ? node_height[nodes_[u].left] : 0;
        uint32_t hr = (nodes_[u].right != kNil && nodes_[u].right_thread == 0) ? node_height[nodes_[u].right] : 0;
        int diff = static_cast<int>(hl) - static_cast<int>(hr);
        if (std::abs(diff) > 1) return false;
        node_height[u] = 1 + std::max(hl, hr);
    }
    return true;
}

} // namespace qroute::unit1