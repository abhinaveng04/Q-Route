#include "qroute/unit5_heap/telemetry_config.hpp"

#include "qroute/common/file_io.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace qroute::unit5 {

namespace {

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string to_lower_cleaned(std::string str) {
    str.erase(std::remove_if(str.begin(), str.end(), [](unsigned char c) {
        return std::isspace(c) || c == '_';
    }), str.end());
    for (char& c : str) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return str;
}

} // namespace

TelemetryConfig TelemetryConfig::load_from_file(const std::string& path) {
    std::string content = read_text_file(path);
    std::istringstream iss(content);
    std::string line;

    TelemetryConfig cfg;

    while (std::getline(iss, line)) {
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            throw std::invalid_argument("Malformed config line (missing '='): " + line);
        }

        std::string raw_key = trim(line.substr(0, eq_pos));
        std::string raw_val = trim(line.substr(eq_pos + 1));

        if (raw_key.empty() || raw_val.empty()) {
            throw std::invalid_argument("Malformed config line (empty key or value): " + line);
        }

        double val = 0.0;
        try {
            val = std::stod(raw_val);
        } catch (const std::exception& e) {
            throw std::invalid_argument("Invalid numeric value in telemetry config: " + raw_val);
        }

        std::string clean_key = to_lower_cleaned(raw_key);
        if (clean_key == "t1us") {
            cfg.t1_us = val;
        } else if (clean_key == "t2us") {
            cfg.t2_us = val;
        } else if (clean_key == "fidelitythreshold") {
            cfg.fidelity_threshold = val;
        } else if (clean_key == "alertslackus") {
            cfg.alert_slack_us = val;
        } else if (clean_key == "singlequbitns") {
            cfg.single_qubit_ns = val;
        } else if (clean_key == "twoqubitns") {
            cfg.two_qubit_ns = val;
        } else if (clean_key == "measurens") {
            cfg.measure_ns = val;
        } else {
            throw std::invalid_argument("Unknown telemetry config key: " + raw_key);
        }
    }

    // Validation
    if (cfg.t1_us <= 0.0) {
        throw std::invalid_argument("t1_us must be strictly positive");
    }
    if (cfg.t2_us <= 0.0) {
        throw std::invalid_argument("t2_us must be strictly positive");
    }
    if (cfg.fidelity_threshold <= 0.0 || cfg.fidelity_threshold > 1.0) {
        throw std::invalid_argument("fidelity_threshold must be in the interval (0, 1]");
    }
    if (cfg.single_qubit_ns < 0.0) {
        throw std::invalid_argument("single_qubit_ns must be non-negative");
    }
    if (cfg.two_qubit_ns < 0.0) {
        throw std::invalid_argument("two_qubit_ns must be non-negative");
    }
    if (cfg.measure_ns < 0.0) {
        throw std::invalid_argument("measure_ns must be non-negative");
    }

    return cfg;
}

} // namespace qroute::unit5
