#pragma once

namespace qroute {

class ScopedTimerUs {
public:
    explicit ScopedTimerUs(double& sink_us);
    ~ScopedTimerUs();
    double elapsed_us() const noexcept;

private:
    double& sink_;
    double  start_us_;
};

double now_us() noexcept;

} // namespace qroute