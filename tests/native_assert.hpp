#pragma once

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace qroute::test {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline bool register_test(const std::string& suite, const std::string& name, std::function<void()> func) {
    get_registry().push_back({suite, name, std::move(func)});
    return true;
}

class TestFailureException : public std::exception {
public:
    explicit TestFailureException(std::string msg) : msg_(std::move(msg)) {}
    const char* what() const noexcept override { return msg_.c_str(); }
private:
    std::string msg_;
};

inline int run_all_tests(int /*argc*/ = 0, char** /*argv*/ = nullptr) {
    int passed = 0;
    int failed = 0;
    const auto& tests = get_registry();
    std::cout << "[==========] Running " << tests.size() << " tests.\n";

    for (const auto& t : tests) {
        std::cout << "[ RUN      ] " << t.suite << "." << t.name << "\n";
        try {
            t.func();
            std::cout << "[       OK ] " << t.suite << "." << t.name << "\n";
            ++passed;
        } catch (const TestFailureException& e) {
            std::cout << "[  FAILED  ] " << t.suite << "." << t.name << "\n";
            std::cout << "             " << e.what() << "\n";
            ++failed;
        } catch (const std::exception& e) {
            std::cout << "[  FAILED  ] " << t.suite << "." << t.name << " (unexpected exception: " << e.what() << ")\n";
            ++failed;
        } catch (...) {
            std::cout << "[  FAILED  ] " << t.suite << "." << t.name << " (unknown exception caught)\n";
            ++failed;
        }
    }

    std::cout << "[==========] " << tests.size() << " tests ran.\n";
    std::cout << "[  PASSED  ] " << passed << " tests.\n";
    if (failed > 0) {
        std::cout << "[  FAILED  ] " << failed << " tests.\n";
        return 1;
    }
    return 0;
}

} // namespace qroute::test

#define TEST_FAIL(msg) \
    do { \
        std::ostringstream _oss; \
        _oss << __FILE__ << ":" << __LINE__ << ": Assertion failed: " << msg; \
        throw ::qroute::test::TestFailureException(_oss.str()); \
    } while (0)

#define ASSERT_TRUE(cond) \
    do { if (!(cond)) { TEST_FAIL("Expected (" #cond ") to be true"); } } while (0)

#define ASSERT_FALSE(cond) \
    do { if (cond) { TEST_FAIL("Expected (" #cond ") to be false"); } } while (0)

#define ASSERT_EQ(a, b) \
    do { if (!((a) == (b))) { \
        std::ostringstream _ss; \
        _ss << "Expected (" #a " == " #b "), but (" << (a) << " != " << (b) << ")"; \
        TEST_FAIL(_ss.str()); \
    } } while (0)

#define ASSERT_NE(a, b) \
    do { if ((a) == (b)) { \
        std::ostringstream _ss; \
        _ss << "Expected (" #a " != " #b "), but both equal (" << (a) << ")"; \
        TEST_FAIL(_ss.str()); \
    } } while (0)

#define ASSERT_LT(a, b) \
    do { if (!((a) < (b))) { \
        std::ostringstream _ss; \
        _ss << "Expected (" #a " < " #b "), but (" << (a) << " >= " << (b) << ")"; \
        TEST_FAIL(_ss.str()); \
    } } while (0)

#define ASSERT_LE(a, b) \
    do { if (!((a) <= (b))) { \
        std::ostringstream _ss; \
        _ss << "Expected (" #a " <= " #b "), but (" << (a) << " > " << (b) << ")"; \
        TEST_FAIL(_ss.str()); \
    } } while (0)

#define ASSERT_GT(a, b) \
    do { if (!((a) > (b))) { \
        std::ostringstream _ss; \
        _ss << "Expected (" #a " > " #b "), but (" << (a) << " <= " << (b) << ")"; \
        TEST_FAIL(_ss.str()); \
    } } while (0)

#define ASSERT_GE(a, b) \
    do { if (!((a) >= (b))) { \
        std::ostringstream _ss; \
        _ss << "Expected (" #a " >= " #b "), but (" << (a) << " < " << (b) << ")"; \
        TEST_FAIL(_ss.str()); \
    } } while (0)

#define ASSERT_NEAR(a, b, eps) \
    do { if (std::abs((a) - (b)) > (eps)) { \
        std::ostringstream _ss; \
        _ss << "Expected |(" #a ") - (" #b ")| <= " << (eps) << ", but diff is " << std::abs((a) - (b)); \
        TEST_FAIL(_ss.str()); \
    } } while (0)

#define ASSERT_THROW(statement, exception_type) \
    do { \
        bool _threw = false; \
        try { statement; } \
        catch (const exception_type&) { _threw = true; } \
        catch (...) { TEST_FAIL("Threw unexpected exception type for: " #statement); } \
        if (!_threw) { TEST_FAIL("Expected exception " #exception_type " for: " #statement); } \
    } while (0)

#define ASSERT_NO_THROW(statement) \
    do { \
        try { statement; } \
        catch (const std::exception& _e) { \
            TEST_FAIL(std::string("Unexpected exception: ") + _e.what()); \
        } catch (...) { \
            TEST_FAIL("Unexpected non-std::exception thrown"); \
        } \
    } while (0)

#define EXPECT_TRUE(cond) ASSERT_TRUE(cond)
#define EXPECT_FALSE(cond) ASSERT_FALSE(cond)
#define EXPECT_EQ(a, b) ASSERT_EQ(a, b)
#define EXPECT_NE(a, b) ASSERT_NE(a, b)
#define EXPECT_LT(a, b) ASSERT_LT(a, b)
#define EXPECT_LE(a, b) ASSERT_LE(a, b)
#define EXPECT_GT(a, b) ASSERT_GT(a, b)
#define EXPECT_GE(a, b) ASSERT_GE(a, b)
#define EXPECT_NEAR(a, b, eps) ASSERT_NEAR(a, b, eps)
#define EXPECT_THROW(stmt, exc) ASSERT_THROW(stmt, exc)
#define EXPECT_NO_THROW(stmt) ASSERT_NO_THROW(stmt)

#define TEST(suite, name) \
    void test_##suite##_##name(); \
    static const bool reg_##suite##_##name = \
        ::qroute::test::register_test(#suite, #name, test_##suite##_##name); \
    void test_##suite##_##name()