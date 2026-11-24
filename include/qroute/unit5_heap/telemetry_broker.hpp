#pragma once

#include "qroute/common/constants.hpp"
#include "qroute/common/types.hpp"
#include "qroute/unit5_heap/fibonacci_heap.hpp"
#include "qroute/unit5_heap/telemetry_config.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace qroute::unit5 {

struct Alert {
    PhysicalQubit qubit       = kNil;
    double        slack_us    = 0.0;
    double        elapsed_us  = 0.0;
    double        hazard_rate = 0.0;
    uint64_t      gate_index  = 0;
};

struct QubitHealth {
    double elapsed_ns       = 0.0;
    double t1_us            = kDefaultT1Us;
    double t2_us            = kDefaultT2Us;
    double slack_us         = kInfinity;
    bool   active           = false;
    FibonacciHeap<std::pair<double, uint64_t>, PhysicalQubit>::Handle handle = nullptr;
    uint64_t last_gate_index = 0;
};

class TelemetryBroker {
public:
    TelemetryBroker(uint32_t physical_qubit_count, TelemetryConfig cfg);

    // Called by the execution engine after every emitted physical gate.
    void on_gate_executed(PhysicalQubit q, double duration_ns, uint64_t gate_index);
    void ongateexecuted(PhysicalQubit q, double duration_ns, uint64_t gate_index) {
        on_gate_executed(q, duration_ns, gate_index);
    }

    void advance_global_time(double duration_ns);
    void advanceglobaltime(double duration_ns) {
        advance_global_time(duration_ns);
    }

    double elapsed_ns(PhysicalQubit q) const noexcept;
    double global_elapsed_ns() const noexcept;
    double globalelapsedns() const noexcept {
        return global_elapsed_ns();
    }
    double slack_us(PhysicalQubit q) const noexcept;

    // Recomputes slack and performs insert or decrease_key [O(1) amortized].
    void update_slack(PhysicalQubit q);
    void updateslack(PhysicalQubit q) {
        update_slack(q);
    }

    bool has_critical_alert() const noexcept;
    bool hascriticalalert() const noexcept {
        return has_critical_alert();
    }

    Alert extract_most_critical();
    Alert extractmostcritical() {
        return extract_most_critical();
    }

    // All alerts with slack <= alert_slack_us, sorted ascending by (slack_us, qubit)
    std::vector<Alert> drain_critical();
    std::vector<Alert> draincritical() {
        return drain_critical();
    }

    FibonacciHeap<std::pair<double, uint64_t>, PhysicalQubit>::Stats heap_stats() const noexcept;
    FibonacciHeap<std::pair<double, uint64_t>, PhysicalQubit>::Stats heapstats() const noexcept {
        return heap_stats();
    }

    const TelemetryConfig& config() const noexcept { return cfg_; }
    uint32_t qubit_count() const noexcept { return static_cast<uint32_t>(health_.size()); }
    uint64_t decrease_key_count() const noexcept { return decrease_key_count_; }
    uint64_t insert_count() const noexcept { return insert_count_; }

    double hazard_rate(PhysicalQubit q) const noexcept;
    double critical_time_us(PhysicalQubit q) const noexcept;

private:
    TelemetryConfig                                           cfg_;
    std::vector<QubitHealth>                                  health_;
    FibonacciHeap<std::pair<double, uint64_t>, PhysicalQubit> heap_; // key = (slack_us, insertion_seq)
    uint64_t                                                  insertion_seq_ = 0;
    double                                                    global_elapsed_ns_ = 0.0;
    uint64_t                                                  decrease_key_count_ = 0;
    uint64_t                                                  insert_count_ = 0;

    double hazard_rate(const QubitHealth& h) const noexcept;
    double critical_time_us(const QubitHealth& h) const noexcept;
};

} // namespace qroute::unit5
