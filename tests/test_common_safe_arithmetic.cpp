#include "native_assert.hpp"
#include "qroute/common/constants.hpp"
#include "qroute/common/deterministic_time.hpp"
#include "qroute/common/file_io.hpp"
#include "qroute/common/safe_arithmetic.hpp"
#include "qroute/common/types.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace qroute;

// T0.1: SafeArithmetic::multiply_u64 boundary (0, 1, MAX, MAX/2, overflow cases)
TEST(CommonSafeArithmetic, T0_1_MultiplyU64Boundary) {
    uint64_t out = 0;
    constexpr uint64_t kMax = SafeArithmetic::kMaxU64;

    // 0 * anything = 0
    ASSERT_FALSE(SafeArithmetic::multiply_u64(0, 0, out));
    ASSERT_EQ(out, 0ULL);
    ASSERT_FALSE(SafeArithmetic::multiply_u64(0, kMax, out));
    ASSERT_EQ(out, 0ULL);
    ASSERT_FALSE(SafeArithmetic::multiply_u64(kMax, 0, out));
    ASSERT_EQ(out, 0ULL);

    // 1 * X = X
    ASSERT_FALSE(SafeArithmetic::multiply_u64(1, 42, out));
    ASSERT_EQ(out, 42ULL);
    ASSERT_FALSE(SafeArithmetic::multiply_u64(1, kMax, out));
    ASSERT_EQ(out, kMax);
    ASSERT_FALSE(SafeArithmetic::multiply_u64(kMax, 1, out));
    ASSERT_EQ(out, kMax);

    // Regular multiplication
    ASSERT_FALSE(SafeArithmetic::multiply_u64(10, 20, out));
    ASSERT_EQ(out, 200ULL);

    // Boundary: (kMax / 2) * 2
    uint64_t half_max = kMax / 2;
    ASSERT_FALSE(SafeArithmetic::multiply_u64(half_max, 2, out));
    ASSERT_EQ(out, half_max * 2);

    // Overflow: (kMax / 2 + 1) * 2
    ASSERT_TRUE(SafeArithmetic::multiply_u64(half_max + 1, 2, out));
    ASSERT_EQ(out, kMax);

    // Overflow: kMax * 2
    ASSERT_TRUE(SafeArithmetic::multiply_u64(kMax, 2, out));
    ASSERT_EQ(out, kMax);
}

// T0.2: SafeArithmetic::add_u64 boundary and saturation
TEST(CommonSafeArithmetic, T0_2_AddU64Boundary) {
    uint64_t out = 0;
    constexpr uint64_t kMax = SafeArithmetic::kMaxU64;

    // 0 + X = X
    ASSERT_FALSE(SafeArithmetic::add_u64(0, 0, out));
    ASSERT_EQ(out, 0ULL);
    ASSERT_FALSE(SafeArithmetic::add_u64(0, kMax, out));
    ASSERT_EQ(out, kMax);
    ASSERT_FALSE(SafeArithmetic::add_u64(kMax, 0, out));
    ASSERT_EQ(out, kMax);

    // Regular addition
    ASSERT_FALSE(SafeArithmetic::add_u64(100, 250, out));
    ASSERT_EQ(out, 350ULL);

    // kMax - 1 + 1 = kMax (no overflow)
    ASSERT_FALSE(SafeArithmetic::add_u64(kMax - 1, 1, out));
    ASSERT_EQ(out, kMax);

    // Overflow: kMax + 1
    ASSERT_TRUE(SafeArithmetic::add_u64(kMax, 1, out));
    ASSERT_EQ(out, kMax);

    // Overflow: kMax + kMax
    ASSERT_TRUE(SafeArithmetic::add_u64(kMax, kMax, out));
    ASSERT_EQ(out, kMax);
}

// T0.3: saturated_triple_product_add chained overflow propagation
TEST(CommonSafeArithmetic, T0_3_SaturatedTripleProductAdd) {
    uint64_t out = 0;
    constexpr uint64_t kMax = SafeArithmetic::kMaxU64;

    // 2 * 3 * 4 + 10 = 24 + 10 = 34
    ASSERT_FALSE(SafeArithmetic::saturated_triple_product_add(2, 3, 4, 10, out));
    ASSERT_EQ(out, 34ULL);

    // a * b overflows
    ASSERT_TRUE(SafeArithmetic::saturated_triple_product_add(kMax, 2, 1, 0, out));
    ASSERT_EQ(out, kMax);

    // (a * b) * c overflows
    uint64_t big = 1ULL << 32;
    ASSERT_TRUE(SafeArithmetic::saturated_triple_product_add(big, big, 2, 0, out));
    ASSERT_EQ(out, kMax);

    // base + (a*b*c) overflows
    ASSERT_TRUE(SafeArithmetic::saturated_triple_product_add(10, 10, 10, kMax - 500, out));
    ASSERT_EQ(out, kMax);
}

// T0.4: grep-based assertion that no source contains 128-bit extension types [INV-4]
TEST(CommonSafeArithmetic, T0_4_Inv4No128BitTypes) {
    std::vector<std::string> test_files = {
        "include/qroute/common/safe_arithmetic.hpp",
        "src/common/safe_arithmetic.cpp",
        "include/qroute/common/types.hpp",
        "include/qroute/common/constants.hpp"
    };

    std::string s128 = std::string("1") + "28";
    std::vector<std::string> forbidden = {
        "__int" + s128, "_umul" + s128, "uint" + s128 + "_t", "int" + s128 + "_t", "uint" + s128 + "t", "int" + s128
    };

    for (const auto& path : test_files) {
        std::string content = read_text_file(path);
        for (const auto& f : forbidden) {
            ASSERT_EQ(content.find(f), std::string::npos);
        }
    }
}

// T0.5: fnv1a64 known-answer tests (empty string, 'a', 'foobar')
TEST(CommonSafeArithmetic, T0_5_Fnv1a64KnownAnswer) {
    ASSERT_EQ(SafeArithmetic::fnv1a64str(""), 1469598103934665603ULL);
    ASSERT_EQ(SafeArithmetic::fnv1a64str("a"), 4953267810257967366ULL);
    ASSERT_EQ(SafeArithmetic::fnv1a64str("foobar"), 9870438755804841970ULL);
}

// T0.6: file_io round-trip write/read byte equality
TEST(CommonSafeArithmetic, T0_6_FileIoRoundTrip) {
    std::string test_data = "Hello Quantum World!\nLine 2 with \t tabs.\nFinal line without trailing newline.";
    std::string tmp_path = "build_test_io_roundtrip.tmp";

    ASSERT_TRUE(write_text_file(tmp_path, test_data));
    std::string read_back = read_text_file(tmp_path);
    ASSERT_EQ(test_data, read_back);
}

// T0.7: read_lines 1-based numbering and CRLF stripping
TEST(CommonSafeArithmetic, T0_7_ReadLines1BasedAndCRLF) {
    std::string test_crlf = "Line One\r\nLine Two\r\nLine Three\r\n";
    std::string tmp_path = "build_test_crlf.tmp";
    ASSERT_TRUE(write_text_file(tmp_path, test_crlf));

    std::vector<std::string> lines = read_lines(tmp_path);
    // 1-based indexing: lines[0] is dummy empty string
    ASSERT_EQ(lines.size(), 4ULL); // dummy + 3 lines
    ASSERT_EQ(lines[0], "");
    ASSERT_EQ(lines[1], "Line One");
    ASSERT_EQ(lines[2], "Line Two");
    ASSERT_EQ(lines[3], "Line Three");

    // Check strip_comment
    ASSERT_EQ(strip_comment("h q[0]; // Hadamard gate"), "h q[0]; ");
    ASSERT_EQ(strip_comment("# Top level comment"), "");
    ASSERT_EQ(strip_comment("cx q[0], q[1];"), "cx q[0], q[1];");
}

// T0.8: data/ files exist, are non-empty, and parse their header lines correctly
TEST(CommonSafeArithmetic, T0_8_DataFilesExistAndHeaders) {
    // circuit.qasm
    auto circuit_lines = read_lines("data/circuit.qasm");
    ASSERT_GE(circuit_lines.size(), 2ULL);
    ASSERT_EQ(circuit_lines[1], "OPENQASM 2.0;");

    // coupling.map
    auto coupling_lines = read_lines("data/coupling.map");
    ASSERT_GE(coupling_lines.size(), 3ULL);
    auto tokens = split_tokens(coupling_lines[2]);
    ASSERT_EQ(tokens.size(), 2ULL);
    ASSERT_EQ(tokens[0], "5");
    ASSERT_EQ(tokens[1], "4");

    // coupling_grid9.map
    auto grid_lines = read_lines("data/coupling_grid9.map");
    ASSERT_GE(grid_lines.size(), 3ULL);
    tokens = split_tokens(grid_lines[2]);
    ASSERT_EQ(tokens.size(), 2ULL);
    ASSERT_EQ(tokens[0], "9");
    ASSERT_EQ(tokens[1], "12");

    // coupling_heavyhex7.map
    auto hex_lines = read_lines("data/coupling_heavyhex7.map");
    ASSERT_GE(hex_lines.size(), 3ULL);
    tokens = split_tokens(hex_lines[2]);
    ASSERT_EQ(tokens.size(), 2ULL);
    ASSERT_EQ(tokens[0], "7");
    ASSERT_EQ(tokens[1], "7");

    // contraction.chain
    auto chain_lines = read_lines("data/contraction.chain");
    ASSERT_GE(chain_lines.size(), 3ULL);
    tokens = split_tokens(chain_lines[2]);
    ASSERT_EQ(tokens.size(), 1ULL);
    ASSERT_EQ(tokens[0], "4");

    // telemetry.conf
    std::string telemetry_content = read_text_file("data/telemetry.conf");
    ASSERT_FALSE(telemetry_content.empty());
    ASSERT_NE(telemetry_content.find("t1_us"), std::string::npos);
}

int main(int argc, char** argv) {
    return qroute::test::run_all_tests(argc, argv);
}