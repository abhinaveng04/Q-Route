#pragma once

#include "qroute/common/types.hpp"
#include "qroute/unit1_ast/ast_node.hpp"
#include "qroute/unit1_ast/gate_payload.hpp"

#include <cstddef>
#include <vector>

namespace qroute::unit1 {

class ThreadedAST {
public:
    ThreadedAST() = default;

    void reserve(size_t node_count);

    // Appends a node in source order. Returns its index.
    NodeIndex append(OpCode op, GatePayload payload, uint32_t source_line);

    // Builds a balanced binary projection over [begin, end). Inorder == source order.
    NodeIndex build_balanced(NodeIndex begin, NodeIndex end);
    void      build_from_sequence();          // build_balanced(0, size())
    void      buildfromsequence() { build_from_sequence(); }

    // Converts every absent child into an inorder predecessor/successor thread.
    void finalize_threads();

    NodeIndex root() const noexcept { return root_; }
    size_t    size() const noexcept { return nodes_.size(); }

    const ASTNode&     node(NodeIndex i) const;
    const GatePayload& payload(NodeIndex i) const;

    // ---- O(1)-auxiliary iterative traversal primitives ----
    NodeIndex leftmost(NodeIndex start) const noexcept;
    NodeIndex next(NodeIndex i) const noexcept;
    NodeIndex prev(NodeIndex i) const noexcept;   // symmetric, backwards traversal

    template <typename Fn>
    void traverse(Fn fn) const {
        for (NodeIndex cur = leftmost(root_); cur != kNil; cur = next(cur)) {
            fn(cur, node(cur), payload(node(cur).payload));
        }
    }

    // Test/validation helpers (may use auxiliary memory; NOT used in hot path)
    std::vector<NodeIndex> inorder_reference() const;   // stack-based oracle
    bool validate_threads() const noexcept;
    uint32_t height() const noexcept;
    bool is_balanced() const noexcept;                  // |hL - hR| <= 1

private:
    std::vector<ASTNode>     nodes_;
    std::vector<GatePayload> payloads_;
    NodeIndex                root_ = kNil;
};

} // namespace qroute::unit1