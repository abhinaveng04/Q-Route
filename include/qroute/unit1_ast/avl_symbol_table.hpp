#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace qroute::unit1 {

template <typename Value>
class AvlSymbolTable {
public:
    struct Entry {
        std::string key;
        Value       value{};
    };

    AvlSymbolTable() = default;

    // Disallow copy to preserve unique_ptr ownership semantics
    AvlSymbolTable(const AvlSymbolTable&) = delete;
    AvlSymbolTable& operator=(const AvlSymbolTable&) = delete;
    AvlSymbolTable(AvlSymbolTable&&) noexcept = default;
    AvlSymbolTable& operator=(AvlSymbolTable&&) noexcept = default;

    bool insert(const std::string& key, Value value) {
        bool inserted = false;
        root_ = insert_internal(root_, key, std::move(value), inserted);
        return inserted;
    }

    bool contains(const std::string& key) const noexcept {
        return find(key) != nullptr;
    }

    const Value* find(const std::string& key) const noexcept {
        const Node* curr = root_;
        while (curr) {
            if (key < curr->key) {
                curr = curr->left;
            } else if (curr->key < key) {
                curr = curr->right;
            } else {
                return &curr->value;
            }
        }
        return nullptr;
    }

    Value* find(const std::string& key) noexcept {
        Node* curr = root_;
        while (curr) {
            if (key < curr->key) {
                curr = curr->left;
            } else if (curr->key < key) {
                curr = curr->right;
            } else {
                return &curr->value;
            }
        }
        return nullptr;
    }

    size_t size() const noexcept {
        return pool_.size();
    }

    uint32_t height() const noexcept {
        return root_ ? static_cast<uint32_t>(root_->height) : 0;
    }

    // A-4: Explicit iterative check for tests
    bool is_balanced() const noexcept {
        if (!root_) return true;
        std::vector<const Node*> st;
        st.reserve(32);
        st.push_back(root_);
        while (!st.empty()) {
            const Node* n = st.back();
            st.pop_back();
            int balance = bf(n);
            if (balance < -1 || balance > 1) {
                return false;
            }
            if (n->left) st.push_back(n->left);
            if (n->right) st.push_back(n->right);
        }
        return true;
    }

    std::vector<Entry> inorder() const {
        std::vector<Entry> result;
        result.reserve(pool_.size());
        std::vector<const Node*> st;
        st.reserve(32);
        const Node* curr = root_;
        while (curr || !st.empty()) {
            while (curr) {
                st.push_back(curr);
                curr = curr->left;
            }
            curr = st.back();
            st.pop_back();
            result.push_back(Entry{curr->key, curr->value});
            curr = curr->right;
        }
        return result;
    }

private:
    struct Node {
        std::string key;
        Value       value{};
        Node*       left   = nullptr; // non-owning structural link
        Node*       right  = nullptr;
        int32_t     height = 1;
    };

    std::vector<std::unique_ptr<Node>> pool_; // OWNS all nodes (RAII, zero raw new/delete)
    Node* root_ = nullptr;

    static int32_t h(const Node* n) noexcept {
        return n ? n->height : 0;
    }

    static int32_t bf(const Node* n) noexcept {
        return n ? (h(n->left) - h(n->right)) : 0;
    }

    static void update_height(Node* n) noexcept {
        if (n) {
            n->height = 1 + std::max(h(n->left), h(n->right));
        }
    }

    Node* rotate_right(Node* y) noexcept {
        Node* x  = y->left;
        Node* t2 = x->right;
        x->right = y;
        y->left  = t2;
        update_height(y);
        update_height(x);
        return x;
    }

    Node* rotate_left(Node* x) noexcept {
        Node* y  = x->right;
        Node* t2 = y->left;
        y->left  = x;
        x->right = t2;
        update_height(x);
        update_height(y);
        return y;
    }

    Node* rebalance(Node* n) noexcept {
        if (!n) return nullptr;
        update_height(n);
        int balance = bf(n);

        // Left Heavy: LL or LR
        if (balance > 1) {
            if (bf(n->left) < 0) {
                // LR case
                n->left = rotate_left(n->left);
            }
            // LL case
            return rotate_right(n);
        }

        // Right Heavy: RR or RL
        if (balance < -1) {
            if (bf(n->right) > 0) {
                // RL case
                n->right = rotate_right(n->right);
            }
            // RR case
            return rotate_left(n);
        }

        return n;
    }

    Node* insert_internal(Node* node, const std::string& key, Value value, bool& inserted) {
        if (!node) {
            pool_.push_back(std::make_unique<Node>());
            Node* new_node = pool_.back().get();
            new_node->key    = key;
            new_node->value  = std::move(value);
            new_node->left   = nullptr;
            new_node->right  = nullptr;
            new_node->height = 1;
            inserted = true;
            return new_node;
        }

        if (key < node->key) {
            node->left = insert_internal(node->left, key, std::move(value), inserted);
        } else if (node->key < key) {
            node->right = insert_internal(node->right, key, std::move(value), inserted);
        } else {
            inserted = false;
            return node;
        }

        return rebalance(node);
    }
};

} // namespace qroute::unit1