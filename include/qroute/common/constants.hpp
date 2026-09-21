#pragma once

#include <cstdint>
#include <limits>

namespace qroute {

inline constexpr double kEpsilon                  = 1e-12;
inline constexpr double kInfinity                 = std::numeric_limits<double>::infinity();
inline constexpr double kDefaultT1Us              = 50.0;
inline constexpr double kDefaultT2Us              = 70.0;
inline constexpr double kDefaultFidelityThreshold = 0.90;
inline constexpr double kDefaultAlertSlackUs      = 5.0;
inline constexpr double kSingleQubitNs            = 35.0;
inline constexpr double kTwoQubitNs               = 300.0;
inline constexpr double kMeasureNs                = 800.0;
inline constexpr uint32_t kMaxMcmDimShift         = 20; // cap 2^20 in derived chains

} // namespace qroute