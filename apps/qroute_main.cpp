#include "qroute/common/file_io.hpp"
#include "qroute/pipeline/benchmark_suite.hpp"
#include "qroute/pipeline/execution_engine.hpp"

#include <fstream>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

namespace {

void print_usage(std::ostream& out) {
    out << "Usage: qroute [options]\n\n"
        << "Required Options (unless --bench alone):\n"
        << "  --circuit <file>       OpenQASM 2.0 input trace\n"
        << "  --coupling <file>      Physical coupling map\n"
        << "  --out <file>           Routed physical circuit output path\n\n"
        << "Execution & Tuning Options:\n"
        << "  --bench                Run empirical benchmark suite (Section D.8)\n"
        << "  --chain <file>         Explicit MCM dimension chain file\n"
        << "  --telemetry <file>     Telemetry configuration file (telemetry.conf)\n"
        << "  --lookahead <window>   Two-qubit gate lookahead window (default: 0)\n"
        << "  --time-limit <ms>      B&B search time budget in ms (default: 1000.0)\n"
        << "  --node-budget <nodes>  B&B expanded-node budget (default: 0, unlimited)\n"
        << "  --out-json <file>      Machine-readable JSON report output path\n"
        << "  --verbose              Emit per-phase trace lines to stderr\n"
        << "  --version              Print version (qroute 1.0.0 (R2)) and exit 0\n"
        << "  --help, -h             Print this usage message and exit 0\n";
}

} // namespace

int main(int argc, char* argv[]) {
    // Install classic C locale at main entry (PRD D.1, F.4)
    std::locale::global(std::locale::classic());

    qroute::pipeline::EngineConfig config;
    bool has_bench = false;
    bool has_circuit = false;
    bool has_coupling = false;
    bool has_out = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(std::cout);
            return 0;
        }
        if (arg == "--version") {
            std::cout << "qroute 1.0.0 (R2)\n";
            return 0;
        }
        if (arg == "--bench") {
            has_bench = true;
            config.run_benchmarks = true;
            continue;
        }
        if (arg == "--verbose") {
            config.verbose = true;
            continue;
        }

        // Flags requiring arguments
        if (arg == "--circuit") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --circuit requires a file argument\n";
                return 2;
            }
            config.circuit_path = argv[++i];
            has_circuit = true;
        } else if (arg == "--coupling") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --coupling requires a file argument\n";
                return 2;
            }
            config.coupling_path = argv[++i];
            has_coupling = true;
        } else if (arg == "--out") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --out requires a file argument\n";
                return 2;
            }
            config.out_path = argv[++i];
            has_out = true;
        } else if (arg == "--chain") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --chain requires a file argument\n";
                return 2;
            }
            config.chain_path = argv[++i];
        } else if (arg == "--telemetry") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --telemetry requires a file argument\n";
                return 2;
            }
            config.telemetry_path = argv[++i];
        } else if (arg == "--out-json") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --out-json requires a file argument\n";
                return 2;
            }
            config.out_json_path = argv[++i];
        } else if (arg == "--lookahead") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --lookahead requires an integer argument\n";
                return 2;
            }
            try {
                config.lookahead_window = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Error: Invalid integer for --lookahead\n";
                return 2;
            }
        } else if (arg == "--time-limit") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --time-limit requires a numeric argument\n";
                return 2;
            }
            try {
                config.placement_time_limit_ms = std::stod(argv[++i]);
            } catch (...) {
                std::cerr << "Error: Invalid number for --time-limit\n";
                return 2;
            }
        } else if (arg == "--node-budget") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --node-budget requires an integer argument\n";
                return 2;
            }
            try {
                config.node_budget = std::stoull(argv[++i]);
            } catch (...) {
                std::cerr << "Error: Invalid integer for --node-budget\n";
                return 2;
            }
        } else {
            std::cerr << "Error: Unknown argument '" << arg << "'\n";
            print_usage(std::cerr);
            return 2;
        }
    }

    // Benchmark standalone mode
    if (has_bench && !has_circuit) {
        int bench_ret = qroute::pipeline::BenchmarkSuite::run_all(std::cout, "build/bench_results.csv");
        return (bench_ret == 0) ? 0 : 7;
    }

    // Validate required input flags
    if (!has_circuit || !has_coupling || !has_out) {
        std::cerr << "Error: Missing required arguments (--circuit, --coupling, --out)\n";
        print_usage(std::cerr);
        return 2;
    }

    auto file_is_readable = [](const std::string& path) {
        std::ifstream f(path);
        return f.good();
    };

    // Check readability of input files
    if (!file_is_readable(config.circuit_path)) {
        std::cerr << "Error: Cannot read circuit file '" << config.circuit_path << "'\n";
        return 2;
    }
    if (!file_is_readable(config.coupling_path)) {
        std::cerr << "Error: Cannot read coupling file '" << config.coupling_path << "'\n";
        return 2;
    }
    if (!config.chain_path.empty() && !file_is_readable(config.chain_path)) {
        std::cerr << "Error: Cannot read chain file '" << config.chain_path << "'\n";
        return 2;
    }
    if (!config.telemetry_path.empty() && !file_is_readable(config.telemetry_path)) {
        std::cerr << "Error: Cannot read telemetry file '" << config.telemetry_path << "'\n";
        return 2;
    }

    // Run execution engine
    qroute::pipeline::ExecutionEngine engine(config);
    int exit_code = engine.run();

    if (exit_code != 0) {
        return exit_code;
    }

    // If --bench was also requested along with routing
    if (has_bench) {
        int bench_ret = qroute::pipeline::BenchmarkSuite::run_all(std::cout, "build/bench_results.csv");
        if (bench_ret != 0) {
            return 7;
        }
    }

    return 0;
}
