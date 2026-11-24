#pragma once

#include "qroute/common/constants.hpp"

#include <string>

namespace qroute::unit5 {

struct TelemetryConfig {
    double t1_us              = kDefaultT1Us;
    double t2_us              = kDefaultT2Us;
    double fidelity_threshold = kDefaultFidelityThreshold;
    union {
        double alert_slack_us = kDefaultAlertSlackUs;
        double alertslackus;
    };
    union {
        double single_qubit_ns = kSingleQubitNs;
        double singlequbitns;
    };
    union {
        double two_qubit_ns = kTwoQubitNs;
        double twoqubitns;
    };
    double measure_ns = kMeasureNs;

    static TelemetryConfig load_from_file(const std::string& path);
    static TelemetryConfig loadfromfile(const std::string& path) {
        return load_from_file(path);
    }
};

} // namespace qroute::unit5
