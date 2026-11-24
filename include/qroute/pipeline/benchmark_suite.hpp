#pragma once

#include <iostream>
#include <string>

namespace qroute::pipeline {

class BenchmarkSuite {
public:
    // Runs all benchmarks [B1]-[B6], prints to out and writes to csv_path
    static int run_all(std::ostream& out = std::cout, const std::string& csv_path = "build/bench_results.csv");

    static void run_b1(std::ostream& out, std::ostream* csv_out = nullptr);
    static void run_b2(std::ostream& out, std::ostream* csv_out = nullptr);
    static void run_b3(std::ostream& out, std::ostream* csv_out = nullptr);
    static void run_b4(std::ostream& out, std::ostream* csv_out = nullptr);
    static void run_b5(std::ostream& out, std::ostream* csv_out = nullptr);
    static void run_b6(std::ostream& out);
};

} // namespace qroute::pipeline
