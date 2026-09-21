#include "qroute/common/deterministic_time.hpp"

#include <chrono>

namespace qroute {

double now_us() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double, std::micro>(now).count();
}

ScopedTimerUs::ScopedTimerUs(double& sink_us)
    : sink_(sink_us), start_us_(now_us()) {}

ScopedTimerUs::~ScopedTimerUs() {
    sink_ = elapsed_us();
}

double ScopedTimerUs::elapsed_us() const noexcept {
    return now_us() - start_us_;
}

} // namespace qroute