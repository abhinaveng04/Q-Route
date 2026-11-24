#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <utility>
#include <vector>

namespace qroute::unit5 {

template <typename Key, typename Value>
class FibonacciHeap {
public:
    struct Node {
        Key     key{};
        Value   value{};
        Node*   parent  = nullptr;
        Node*   child   = nullptr;
        Node*   left    = nullptr; // circular doubly-linked sibling list
        Node*   right   = nullptr;
        int32_t degree  = 0;
        bool    mark    = false;   // cascading-cut marker
        bool    in_heap = false;
    };

    using Handle = Node*;

    struct Stats {
        uint64_t inserts         = 0;
        uint64_t decrease_keys   = 0;
        uint64_t extract_mins    = 0;
        uint64_t cuts            = 0;
        uint64_t cascading_cuts  = 0;
        uint64_t links           = 0;
        uint64_t consolidations  = 0;
        uint64_t nodes_allocated = 0;
        uint64_t nodes_recycled  = 0;
    };

    FibonacciHeap() = default;
    ~FibonacciHeap() = default; // pool_ owns all nodes (RAII)

    // Move/copy operations deleted (handle stability contract, FH-12)
    FibonacciHeap(const FibonacciHeap&)            = delete;
    FibonacciHeap& operator=(const FibonacciHeap&) = delete;
    FibonacciHeap(FibonacciHeap&&)                 = delete;
    FibonacciHeap& operator=(FibonacciHeap&&)      = delete;

    // ---- Core operations ----

    // O(1) amortized lazy insert (FH-2)
    Handle insert(const Key& key, const Value& value) {
        Node* n = allocate(key, value);
        add_to_root_list(n);
        if (min_root_ == nullptr || n->key < min_root_->key) {
            min_root_ = n;
        }
        ++n_;
        ++stats_.inserts;
        return n;
    }

    bool empty() const noexcept {
        return n_ == 0;
    }

    size_t size() const noexcept {
        return n_;
    }

    const Node& minimum() const {
        if (empty() || min_root_ == nullptr) {
            throw std::runtime_error("FibonacciHeap is empty");
        }
        return *min_root_;
    }

    // O(log n) amortized (FH-3)
    std::pair<Key, Value> extract_min() {
        if (empty() || min_root_ == nullptr) {
            throw std::runtime_error("FibonacciHeap is empty");
        }

        Node* z = min_root_;
        Key ret_key = z->key;
        Value ret_val = z->value;

        // Splice all children of z into root list (FH-3)
        if (z->child != nullptr) {
            Node* c = z->child;
            Node* end = c;
            do {
                c->parent = nullptr;
                c = c->right;
            } while (c != end);

            Node* c_left = end->left;
            Node* z_right = z->right;

            z->right = end;
            end->left = z;
            c_left->right = z_right;
            z_right->left = c_left;

            z->child = nullptr;
        }

        // Remove z from root list
        if (z == z->right) {
            min_root_ = nullptr;
        } else {
            min_root_ = z->right;
            remove_from_list(z);
            consolidate();
        }

        --n_;
        recycle(z);
        ++stats_.extract_mins;
        return {ret_key, ret_val};
    }

    // O(1) amortized (FH-6, FH-7)
    void decrease_key(Handle h, const Key& new_key) {
        if (h == nullptr || !h->in_heap) {
            return;
        }
        // Guard against key increase (FH-7)
        if (new_key > h->key) {
            return;
        }

        h->key = new_key;
        Node* y = h->parent;

        if (y != nullptr && h->key < y->key) {
            cut(h, y);
            cascading_cut(y);
        }

        if (min_root_ == nullptr || h->key < min_root_->key) {
            min_root_ = h;
        }

        ++stats_.decrease_keys;
    }

    void decreasekey(Handle h, const Key& new_key) {
        decrease_key(h, new_key);
    }

    // FH-8: returns false for recycled/invalid handles
    bool contains_handle(Handle h) const noexcept {
        if (h == nullptr) {
            return false;
        }
        return h->in_heap;
    }

    bool containshandle(Handle h) const noexcept {
        return contains_handle(h);
    }

    // Copy-in merge (avoids cross-pool pointer aliasing). Other heap is emptied.
    void absorb(FibonacciHeap& other) {
        if (this == &other || other.empty()) {
            return;
        }

        // Copy-in re-insertion
        for (const auto& node : other.pool_) {
            if (node.in_heap) {
                insert(node.key, node.value);
            }
        }

        // Empty other heap
        other.min_root_ = nullptr;
        other.n_ = 0;
        other.free_list_.clear();
        other.pool_.clear();
        other.degree_table_.clear();
    }

    // ---- Introspection for tests/benchmarks ----
    size_t pool_size() const noexcept {
        return pool_.size();
    }
    size_t poolsize() const noexcept {
        return pool_size();
    }

    size_t free_list_size() const noexcept {
        return free_list_.size();
    }
    size_t freelistsize() const noexcept {
        return free_list_size();
    }

    size_t root_list_size() const {
        if (min_root_ == nullptr) {
            return 0;
        }
        size_t count = 0;
        Node* cur = min_root_;
        do {
            ++count;
            cur = cur->right;
            if (count > pool_.size()) {
                return count; // safety break against malformed loop
            }
        } while (cur != min_root_);
        return count;
    }
    size_t rootlistsize() const {
        return root_list_size();
    }

    uint32_t max_degree() const noexcept {
        uint32_t max_d = 0;
        for (const auto& node : pool_) {
            if (node.in_heap && static_cast<uint32_t>(node.degree) > max_d) {
                max_d = static_cast<uint32_t>(node.degree);
            }
        }
        return max_d;
    }
    uint32_t maxdegree() const noexcept {
        return max_degree();
    }

    Stats stats() const noexcept {
        return stats_;
    }

    // FH-9: degree bound <= floor(log_phi(n)) + 1, circular list integrity, heap order
    bool validate_structure() const {
        if (n_ == 0) {
            return min_root_ == nullptr;
        }
        if (min_root_ == nullptr) {
            return false;
        }

        // 1. Max degree bound: max_degree <= floor(log_phi(n)) + 1
        double phi = (1.0 + std::sqrt(5.0)) / 2.0;
        uint32_t degree_bound = static_cast<uint32_t>(
            std::floor(std::log(static_cast<double>(n_)) / std::log(phi))) + 1;
        if (max_degree() > degree_bound) {
            return false;
        }

        // 2. Validate root list circular doubly-linked integrity
        size_t root_count = 0;
        Node* r = min_root_;
        do {
            if (r == nullptr || !r->in_heap || r->parent != nullptr) {
                return false;
            }
            if (r->left->right != r || r->right->left != r) {
                return false;
            }
            if (r->key < min_root_->key) {
                return false; // min_root_ must be <= every root
            }
            ++root_count;
            if (root_count > pool_.size()) {
                return false;
            }
            r = r->right;
        } while (r != min_root_);

        // 3. Validate trees recursively
        size_t total_reachable = 0;
        r = min_root_;
        do {
            if (!validate_subtree(r, nullptr, total_reachable)) {
                return false;
            }
            r = r->right;
        } while (r != min_root_);

        return total_reachable == n_;
    }

private:
    std::deque<Node>           pool_;          // OWNS nodes; references/pointers remain stable (FH-10)
    std::vector<Node*>         free_list_;     // recycled nodes (FH-11)
    std::vector<Node*>         degree_table_;  // reusable scratch for consolidate() (FH-4)
    mutable std::vector<Node*> roots_buffer_;  // reusable scratch for consolidate() root collection
    Node*                      min_root_ = nullptr;
    size_t                     n_ = 0;
    Stats                      stats_{};

    Node* allocate(const Key& key, const Value& value) {
        Node* n = nullptr;
        if (!free_list_.empty()) {
            n = free_list_.back();
            free_list_.pop_back();
            ++stats_.nodes_recycled;
        } else {
            pool_.emplace_back();
            n = &pool_.back();
            ++stats_.nodes_allocated;
        }

        n->key = key;
        n->value = value;
        n->parent = nullptr;
        n->child = nullptr;
        n->left = n;
        n->right = n;
        n->degree = 0;
        n->mark = false;
        n->in_heap = true;
        return n;
    }

    void recycle(Node* n) {
        if (n == nullptr) return;
        n->in_heap = false;
        n->parent = nullptr;
        n->child = nullptr;
        n->left = nullptr;
        n->right = nullptr;
        n->degree = 0;
        n->mark = false;
        free_list_.push_back(n);
    }

    void add_to_root_list(Node* n) {
        if (min_root_ == nullptr) {
            min_root_ = n;
            n->left = n;
            n->right = n;
        } else {
            n->right = min_root_->right;
            n->left = min_root_;
            min_root_->right->left = n;
            min_root_->right = n;
        }
        n->parent = nullptr;
    }

    void remove_from_list(Node* n) {
        n->left->right = n->right;
        n->right->left = n->left;
        n->left = n;
        n->right = n;
    }

    // FH-4: consolidate using reusable degree_table_
    void consolidate() {
        ++stats_.consolidations;
        if (min_root_ == nullptr) {
            return;
        }

        if (degree_table_.size() < 64) {
            degree_table_.assign(64, nullptr);
        }

        // Collect current root list nodes into reusable scratch buffer
        roots_buffer_.clear();
        Node* cur = min_root_;
        do {
            roots_buffer_.push_back(cur);
            cur = cur->right;
        } while (cur != min_root_);

        size_t max_d_seen = 0;
        for (Node* w : roots_buffer_) {
            if (w->parent != nullptr || !w->in_heap) {
                continue; // already linked as child during previous step
            }
            Node* x = w;
            size_t d = static_cast<size_t>(x->degree);

            while (d < degree_table_.size() && degree_table_[d] != nullptr) {
                Node* y = degree_table_[d];
                if (y == x) {
                    break;
                }
                if (y->key < x->key) {
                    std::swap(x, y);
                }
                link(y, x); // y becomes child of x (FH-5)
                degree_table_[d] = nullptr;
                ++d;
            }

            if (d >= degree_table_.size()) {
                degree_table_.resize(d + 8, nullptr);
            }
            degree_table_[d] = x;
            if (d > max_d_seen) {
                max_d_seen = d;
            }
        }

        // Rebuild root list from degree_table_ and find min_root_
        min_root_ = nullptr;
        for (size_t i = 0; i <= max_d_seen && i < degree_table_.size(); ++i) {
            Node* y = degree_table_[i];
            if (y != nullptr) {
                degree_table_[i] = nullptr; // reset slot for next consolidate
                y->parent = nullptr;
                y->left = y;
                y->right = y;
                if (min_root_ == nullptr) {
                    min_root_ = y;
                } else {
                    y->right = min_root_->right;
                    y->left = min_root_;
                    min_root_->right->left = y;
                    min_root_->right = y;
                    if (y->key < min_root_->key) {
                        min_root_ = y;
                    }
                }
            }
        }
    }

    // FH-5: link(child, parent)
    void link(Node* child, Node* parent) {
        ++stats_.links;
        remove_from_list(child);
        child->parent = parent;

        if (parent->child == nullptr) {
            parent->child = child;
            child->left = child;
            child->right = child;
        } else {
            child->right = parent->child->right;
            child->left = parent->child;
            parent->child->right->left = child;
            parent->child->right = child;
        }

        ++parent->degree;
        child->mark = false;
    }

    // FH-6: cut
    void cut(Node* x, Node* y) {
        ++stats_.cuts;
        if (x->right == x) {
            y->child = nullptr;
        } else {
            if (y->child == x) {
                y->child = x->right;
            }
            remove_from_list(x);
        }
        --y->degree;

        add_to_root_list(x);
        x->parent = nullptr;
        x->mark = false;
    }

    // FH-6: cascading_cut
    void cascading_cut(Node* y) {
        Node* z = y->parent;
        if (z != nullptr) {
            if (!y->mark) {
                y->mark = true;
            } else {
                ++stats_.cascading_cuts;
                cut(y, z);
                cascading_cut(z);
            }
        }
    }

    bool validate_subtree(Node* node, Node* expected_parent, size_t& visited_count) const {
        if (node == nullptr || !node->in_heap || node->parent != expected_parent) {
            return false;
        }
        if (node->left->right != node || node->right->left != node) {
            return false;
        }
        if (expected_parent != nullptr && node->key < expected_parent->key) {
            return false; // heap-order violation
        }
        ++visited_count;

        if (node->child != nullptr) {
            int32_t child_count = 0;
            Node* c = node->child;
            do {
                if (!validate_subtree(c, node, visited_count)) {
                    return false;
                }
                ++child_count;
                if (child_count > static_cast<int32_t>(pool_.size())) {
                    return false;
                }
                c = c->right;
            } while (c != node->child);

            if (child_count != node->degree) {
                return false; // degree field mismatch
            }
        } else {
            if (node->degree != 0) {
                return false;
            }
        }

        return true;
    }
};

} // namespace qroute::unit5
