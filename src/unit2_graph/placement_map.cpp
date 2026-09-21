#include "qroute/unit2_graph/placement_map.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace qroute::unit2 {

PlacementMap::PlacementMap(uint32_t logical_count, uint32_t physical_count)
    : logical_to_physical_(logical_count, kNil),
      physical_to_logical_(physical_count, kNil) {}

void PlacementMap::set(LogicalQubit l, PhysicalQubit p) {
    if (l >= logical_to_physical_.size()) {
        throw std::out_of_range("Logical qubit index out of range in PlacementMap::set: " + std::to_string(l));
    }
    if (p >= physical_to_logical_.size()) {
        throw std::out_of_range("Physical qubit index out of range in PlacementMap::set: " + std::to_string(p));
    }
    PhysicalQubit old_p = logical_to_physical_[l];
    if (old_p != kNil && old_p < physical_to_logical_.size()) {
        physical_to_logical_[old_p] = kNil;
    }
    LogicalQubit old_l = physical_to_logical_[p];
    if (old_l != kNil && old_l < logical_to_physical_.size()) {
        logical_to_physical_[old_l] = kNil;
    }
    logical_to_physical_[l] = p;
    physical_to_logical_[p] = l;
}

void PlacementMap::load(const std::vector<int32_t>& logical_to_physical) {
    std::fill(logical_to_physical_.begin(), logical_to_physical_.end(), kNil);
    std::fill(physical_to_logical_.begin(), physical_to_logical_.end(), kNil);
    size_t count = std::min(logical_to_physical.size(), logical_to_physical_.size());
    for (size_t l = 0; l < count; ++l) {
        if (logical_to_physical[l] >= 0) {
            set(static_cast<LogicalQubit>(l), static_cast<PhysicalQubit>(logical_to_physical[l]));
        }
    }
}

void PlacementMap::apply_full(std::vector<PhysicalQubit> logical_to_physical) {
    logical_to_physical_ = std::move(logical_to_physical);
    std::fill(physical_to_logical_.begin(), physical_to_logical_.end(), kNil);
    for (size_t l = 0; l < logical_to_physical_.size(); ++l) {
        PhysicalQubit p = logical_to_physical_[l];
        if (p != kNil && p < physical_to_logical_.size()) {
            physical_to_logical_[p] = static_cast<LogicalQubit>(l);
        }
    }
}

PhysicalQubit PlacementMap::physical_of(LogicalQubit l) const {
    if (l >= logical_to_physical_.size()) return kNil;
    return logical_to_physical_[l];
}

LogicalQubit PlacementMap::logical_of(PhysicalQubit p) const {
    if (p >= physical_to_logical_.size()) return kNil;
    return physical_to_logical_[p];
}

bool PlacementMap::is_adjacent(const PhysicalCouplingGraph& g, LogicalQubit a, LogicalQubit b) const {
    PhysicalQubit pa = physical_of(a);
    PhysicalQubit pb = physical_of(b);
    if (pa == kNil || pb == kNil) return false;
    return g.has_edge(pa, pb);
}

void PlacementMap::swap_physical(PhysicalQubit a, PhysicalQubit b) {
    if (a >= physical_to_logical_.size() || b >= physical_to_logical_.size()) {
        throw std::out_of_range("Physical qubit index out of range in swap_physical");
    }
    if (a == b) return;

    LogicalQubit la = physical_to_logical_[a];
    LogicalQubit lb = physical_to_logical_[b];

    physical_to_logical_[a] = lb;
    physical_to_logical_[b] = la;

    if (la != kNil && la < logical_to_physical_.size()) {
        logical_to_physical_[la] = b;
    }
    if (lb != kNil && lb < logical_to_physical_.size()) {
        logical_to_physical_[lb] = a;
    }
}

const std::vector<PhysicalQubit>& PlacementMap::logical_to_physical() const noexcept {
    return logical_to_physical_;
}

const std::vector<LogicalQubit>& PlacementMap::physical_to_logical() const noexcept {
    return physical_to_logical_;
}

bool PlacementMap::validate_bijection() const noexcept {
    std::vector<bool> seen_phys(physical_to_logical_.size(), false);
    for (size_t l = 0; l < logical_to_physical_.size(); ++l) {
        PhysicalQubit p = logical_to_physical_[l];
        if (p != kNil) {
            if (p >= physical_to_logical_.size()) return false;
            if (seen_phys[p]) return false;
            seen_phys[p] = true;
            if (physical_to_logical_[p] != static_cast<LogicalQubit>(l)) return false;
        }
    }
    for (size_t p = 0; p < physical_to_logical_.size(); ++p) {
        LogicalQubit l = physical_to_logical_[p];
        if (l != kNil) {
            if (l >= logical_to_physical_.size()) return false;
            if (logical_to_physical_[l] != static_cast<PhysicalQubit>(p)) return false;
        }
    }
    return true;
}

uint32_t PlacementMap::logical_count() const noexcept {
    return static_cast<uint32_t>(logical_to_physical_.size());
}

uint32_t PlacementMap::physical_count() const noexcept {
    return static_cast<uint32_t>(physical_to_logical_.size());
}

} // namespace qroute::unit2
