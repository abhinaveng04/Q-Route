#include "qroute/unit5_heap/telemetry_broker.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qroute::unit5 {

TelemetryBroker::TelemetryBroker(uint32_t physical_qubit_count, TelemetryConfig cfg)
    : cfg_(std::move(cfg)), health_(physical_qubit_count)
{
    for (uint32_t i = 0; i < physical_qubit_count; ++i) {
        health_[i].t1_us = cfg_.t1_us;
        health_[i].t2_us = cfg_.t2_us;
        health_[i].elapsed_ns = 0.0;
        health_[i].slack_us = critical_time_us(health_[i]);
        health_[i].active = false;
        health_[i].handle = nullptr;
        health_[i].last_gate_index = 0;
    }
}

void TelemetryBroker::on_gate_executed(PhysicalQubit q, double duration_ns, uint64_t gate_index) {
    if (q >= health_.size()) {
        return;
    }
    health_[q].elapsed_ns += duration_ns;
    health_[q].last_gate_index = gate_index;
    advance_global_time(duration_ns);
    update_slack(q);
}

void TelemetryBroker::advance_global_time(double duration_ns) {
    global_elapsed_ns_ += duration_ns;
}

double TelemetryBroker::elapsed_ns(PhysicalQubit q) const noexcept {
    if (q >= health_.size()) {
        return 0.0;
    }
    return health_[q].elapsed_ns;
}

double TelemetryBroker::global_elapsed_ns() const noexcept {
    return global_elapsed_ns_;
}

double TelemetryBroker::slack_us(PhysicalQubit q) const noexcept {
    if (q >= health_.size()) {
        return kInfinity;
    }
    return health_[q].slack_us;
}

double TelemetryBroker::hazard_rate(PhysicalQubit q) const noexcept {
    if (q >= health_.size()) {
        return 0.0;
    }
    return hazard_rate(health_[q]);
}

double TelemetryBroker::critical_time_us(PhysicalQubit q) const noexcept {
    if (q >= health_.size()) {
        return kInfinity;
    }
    return critical_time_us(health_[q]);
}

double TelemetryBroker::hazard_rate(const QubitHealth& h) const noexcept {
    return (1.0 / h.t1_us) + (1.0 / h.t2_us);
}

double TelemetryBroker::critical_time_us(const QubitHealth& h) const noexcept {
    double r = hazard_rate(h);
    if (r <= 0.0) {
        return kInfinity;
    }
    return -std::log(cfg_.fidelity_threshold) / r;
}

void TelemetryBroker::update_slack(PhysicalQubit q) {
    if (q >= health_.size()) {
        return;
    }

    double crit = critical_time_us(health_[q]);
    double elapsed_u = health_[q].elapsed_ns / 1000.0;
    double s = crit - elapsed_u;
    health_[q].slack_us = s;

    auto key = std::make_pair(s, ++insertion_seq_);

    if (!health_[q].active || health_[q].handle == nullptr || !heap_.contains_handle(health_[q].handle)) {
        health_[q].handle = heap_.insert(key, q);
        health_[q].active = true;
        ++insert_count_;
    } else {
        heap_.decrease_key(health_[q].handle, key);
        ++decrease_key_count_;
    }
}

bool TelemetryBroker::has_critical_alert() const noexcept {
    if (heap_.empty()) {
        return false;
    }
    return heap_.minimum().key.first <= cfg_.alert_slack_us;
}

Alert TelemetryBroker::extract_most_critical() {
    if (heap_.empty()) {
        throw std::runtime_error("No alerts available in TelemetryBroker");
    }

    auto [key, q] = heap_.extract_min();
    health_[q].handle = nullptr;
    health_[q].active = false;

    Alert a;
    a.qubit = q;
    a.slack_us = key.first;
    a.elapsed_us = health_[q].elapsed_ns / 1000.0;
    a.hazard_rate = hazard_rate(health_[q]);
    a.gate_index = health_[q].last_gate_index;
    return a;
}

std::vector<Alert> TelemetryBroker::drain_critical() {
    std::vector<Alert> alerts;
    while (!heap_.empty() && heap_.minimum().key.first <= cfg_.alert_slack_us) {
        alerts.push_back(extract_most_critical());
    }

    // Sort ascending by (slack_us, qubit)
    std::sort(alerts.begin(), alerts.end(), [](const Alert& a, const Alert& b) {
        if (std::abs(a.slack_us - b.slack_us) > 1e-9) {
            return a.slack_us < b.slack_us;
        }
        return a.qubit < b.qubit;
    });

    return alerts;
}

FibonacciHeap<std::pair<double, uint64_t>, PhysicalQubit>::Stats TelemetryBroker::heap_stats() const noexcept {
    return heap_.stats();
}

} // namespace qroute::unit5
