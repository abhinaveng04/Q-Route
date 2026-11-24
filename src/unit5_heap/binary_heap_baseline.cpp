#include "qroute/unit5_heap/binary_heap_baseline.hpp"

namespace qroute::unit5 {

// Explicit template instantiations for common types used in benchmarks and tests
template class BinaryHeapBaseline<double, uint32_t>;
template class BinaryHeapBaseline<std::pair<double, uint64_t>, uint32_t>;

} // namespace qroute::unit5
