#pragma once

#include "qroute/unit5_heap/fibonacci_heap.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace qroute::unit5 {

// Indexed binary heap supporting decrease_key, used as benchmark baseline + test oracle.
// Must NEVER be referenced from unit2_graph/ [INV-1].
template <typename Key, typename Value>
class BinaryHeapBaseline {
public:
    using Handle = uint32_t; // stable slot id

    BinaryHeapBaseline() = default;

    Handle insert(const Key& key, const Value& value) {
        Handle slot = 0;
        if (!free_slots_.empty()) {
            slot = free_slots_.back();
            free_slots_.pop_back();
            index_of_[slot] = heap_.size();
        } else {
            slot = static_cast<Handle>(index_of_.size());
            index_of_.push_back(heap_.size());
        }

        heap_.push_back({key, value, slot});
        sift_up(heap_.size() - 1);
        ++stats_.inserts;
        return slot;
    }

    void decrease_key(Handle h, const Key& new_key) {
        if (h >= index_of_.size() || index_of_[h] == kInvalidIndex) {
            return;
        }
        size_t idx = index_of_[h];
        if (idx >= heap_.size() || new_key > heap_[idx].key) {
            return;
        }

        heap_[idx].key = new_key;
        sift_up(idx);
        ++stats_.decrease_keys;
    }

    void decreasekey(Handle h, const Key& new_key) {
        decrease_key(h, new_key);
    }

    const Key& key_of(Handle h) const {
        return heap_[index_of_[h]].key;
    }

    std::pair<Key, Value> extract_min() {
        if (empty()) {
            throw std::runtime_error("BinaryHeapBaseline is empty");
        }

        Entry min_entry = heap_[0];
        Handle slot = min_entry.slot;
        index_of_[slot] = kInvalidIndex;
        free_slots_.push_back(slot);

        if (heap_.size() == 1) {
            heap_.pop_back();
        } else {
            heap_[0] = heap_.back();
            index_of_[heap_[0].slot] = 0;
            heap_.pop_back();
            sift_down(0);
        }

        ++stats_.extract_mins;
        return {min_entry.key, min_entry.value};
    }

    std::pair<Key, Value> extractmin() {
        return extract_min();
    }

    bool contains_handle(Handle h) const noexcept {
        if (h >= index_of_.size()) {
            return false;
        }
        return index_of_[h] != kInvalidIndex;
    }

    bool empty() const noexcept {
        return heap_.empty();
    }

    size_t size() const noexcept {
        return heap_.size();
    }

    typename FibonacciHeap<Key, Value>::Stats stats() const noexcept {
        return stats_;
    }

private:
    static constexpr size_t kInvalidIndex = static_cast<size_t>(-1);

    struct Entry {
        Key    key;
        Value  value;
        Handle slot;
    };

    std::vector<Entry>  heap_;
    std::vector<size_t> index_of_;  // slot -> index in heap_
    std::vector<Handle> free_slots_; // recycled slot ids
    typename FibonacciHeap<Key, Value>::Stats stats_{};

    void sift_up(size_t idx) {
        while (idx > 0) {
            size_t parent = (idx - 1) / 2;
            if (heap_[idx].key < heap_[parent].key) {
                swap_entries(idx, parent);
                idx = parent;
            } else {
                break;
            }
        }
    }

    void sift_down(size_t idx) {
        size_t n = heap_.size();
        while (true) {
            size_t left = 2 * idx + 1;
            size_t right = 2 * idx + 2;
            size_t smallest = idx;

            if (left < n && heap_[left].key < heap_[smallest].key) {
                smallest = left;
            }
            if (right < n && heap_[right].key < heap_[smallest].key) {
                smallest = right;
            }

            if (smallest != idx) {
                swap_entries(idx, smallest);
                idx = smallest;
            } else {
                break;
            }
        }
    }

    void swap_entries(size_t i, size_t j) {
        std::swap(heap_[i], heap_[j]);
        index_of_[heap_[i].slot] = i;
        index_of_[heap_[j].slot] = j;
    }
};

} // namespace qroute::unit5
