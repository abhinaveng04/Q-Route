#pragma once

#include "qroute/common/types.hpp"
#include "qroute/unit2_graph/physical_coupling_graph.hpp"

#include <cstdint>
#include <vector>

namespace qroute::unit2 {

class PlacementMap {
public:
    PlacementMap(uint32_t logical_count, uint32_t physical_count);

    void set(LogicalQubit l, PhysicalQubit p);
    void load(const std::vector<int32_t>& logical_to_physical); // -1 == unmapped
    void apply_full(std::vector<PhysicalQubit> logical_to_physical);
    void applyfull(std::vector<PhysicalQubit> logical_to_physical) {
        apply_full(std::move(logical_to_physical));
    }

    PhysicalQubit physical_of(LogicalQubit l) const;
    PhysicalQubit physicalof(LogicalQubit l) const { return physical_of(l); }

    LogicalQubit logical_of(PhysicalQubit p) const; // kNil if idle
    LogicalQubit logicalof(PhysicalQubit p) const { return logical_of(p); }

    bool is_adjacent(const PhysicalCouplingGraph& g, LogicalQubit a, LogicalQubit b) const;
    bool isadjacent(const PhysicalCouplingGraph& g, LogicalQubit a, LogicalQubit b) const {
        return is_adjacent(g, a, b);
    }

    // Swaps the two PHYSICAL occupants; updates both directions. Core routing primitive.
    void swap_physical(PhysicalQubit a, PhysicalQubit b);
    void swapphysical(PhysicalQubit a, PhysicalQubit b) { swap_physical(a, b); }

    const std::vector<PhysicalQubit>& logical_to_physical() const noexcept;
    const std::vector<PhysicalQubit>& logicaltophysical() const noexcept { return logical_to_physical(); }

    const std::vector<LogicalQubit>& physical_to_logical() const noexcept;
    const std::vector<LogicalQubit>& physicaltological() const noexcept { return physical_to_logical(); }

    bool validate_bijection() const noexcept; // no two logical qubits share a physical qubit
    bool validatebijection() const noexcept { return validate_bijection(); }

    uint32_t logical_count() const noexcept;
    uint32_t logicalcount() const noexcept { return logical_count(); }

    uint32_t physical_count() const noexcept;
    uint32_t physicalcount() const noexcept { return physical_count(); }

private:
    std::vector<PhysicalQubit> logical_to_physical_;
    std::vector<LogicalQubit> physical_to_logical_;
};

} // namespace qroute::unit2
