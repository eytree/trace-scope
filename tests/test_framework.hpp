/**
 * @file test_framework.hpp
 * @brief Lightweight, dependency-free test framework for trace-scope.
 * 
 * Features:
 * - Automatic test registration via TEST() macro
 * - Command-line filtering to run specific tests
 * - Clear pass/fail reporting
 * - No external dependencies (header-only)
 * 
 * Usage:
 * @code
 * #include "test_framework.hpp"
 * 
 * TEST(my_test) {
 *     TEST_ASSERT(1 + 1 == 2, "Math works");
 * }
 * 
 * int main(int argc, char** argv) {
 *     return run_tests(argc, argv);
 * }
 * @endcode
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <functional>
#include <mutex>
#include <stdexcept>

namespace test_framework {

/**
 * @brief Exception thrown when a test assertion fails.
 */
class AssertionFailure : public std::runtime_error {
public:
    explicit AssertionFailure(const char* msg) : std::runtime_error(msg) {}
};

/**
 * @brief Represents a single test case.
 */
struct TestCase {
    const char* name;                       ///< Test name
    std::function<void()> func;             ///< Test function
    
    TestCase(const char* n, std::function<void()> f) : name(n), func(f) {}
};

/**
 * @brief Global registry of all test cases.
 * 
 * Uses singleton pattern with static initialization to ensure proper
 * initialization order.
 */
class TestRegistry {
public:
    /**
     * @brief Get the singleton instance.
     */
    static TestRegistry& instance() {
        static TestRegistry registry;
        return registry;
    }
    
    /**
     * @brief Register a new test case.
     * 
     * Thread-safe via mutex protection.
     */
    void register_test(const char* name, std::function<void()> func) {
        std::lock_guard<std::mutex> lock(mtx_);
        tests_.emplace_back(name, func);
    }
    
    /**
     * @brief Get all registered tests.
     */
    const std::vector<TestCase>& get_tests() const {
        return tests_;
    }
    
private:
    TestRegistry() = default;
    std::vector<TestCase> tests_;
    std::mutex mtx_;
};

/**
 * @brief Helper class for automatic test registration.
 * 
 * Creates a global instance that registers the test during static initialization.
 */
struct TestRegistrar {
    TestRegistrar(const char* name, std::function<void()> func) {
        TestRegistry::instance().register_test(name, func);
    }
};

/**
 * @brief Print usage information.
 */
inline void print_usage(const char* prog_name) {
    std::printf("Usage: %s [OPTIONS] [TEST_FILTER]\n\n", prog_name);
    std::printf("Options:\n");
    std::printf("  --list         List all registered tests\n");
    std::printf("  --help         Show this help message\n");
    std::printf("\n");
    std::printf("Arguments:\n");
    std::printf("  TEST_FILTER    Run only tests matching this substring (optional)\n");
    std::printf("\n");
    std::printf("Examples:\n");
    std::printf("  %s                    # Run all tests\n", prog_name);
    std::printf("  %s my_test            # Run tests containing 'my_test'\n", prog_name);
    std::printf("  %s --list             # List all tests\n", prog_name);
}

/**
 * @brief List all registered tests.
 */
inline void list_tests() {
    const auto& tests = TestRegistry::instance().get_tests();
    std::printf("Registered tests (%zu total):\n", tests.size());
    for (size_t i = 0; i < tests.size(); ++i) {
        std::printf("  [%zu] %s\n", i + 1, tests[i].name);
    }
}

/**
 * @brief Check if a test name matches the filter.
 * 
 * @param test_name The test name to check
 * @param filter The filter string (substring match)
 * @return true if the test matches (or filter is empty)
 */
inline bool matches_filter(const char* test_name, const char* filter) {
    if (!filter || filter[0] == '\0') {
        return true;  // No filter = match all
    }
    return std::strstr(test_name, filter) != nullptr;
}

/**
 * @brief Run all registered tests (with optional filtering).
 * 
 * @param argc Command-line argument count
 * @param argv Command-line arguments
 * @return 0 if all tests pass, 1 if any test fails
 */
inline int run_tests(int argc, char** argv) {
    // Parse command-line arguments
    const char* filter = nullptr;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (std::strcmp(argv[i], "--list") == 0 || std::strcmp(argv[i], "-l") == 0) {
            list_tests();
            return 0;
        }
        else if (argv[i][0] != '-') {
            // First non-option argument is the filter
            if (!filter) {
                filter = argv[i];
            }
        }
    }
    
    const auto& tests = TestRegistry::instance().get_tests();
    
    if (tests.empty()) {
        std::printf("No tests registered!\n");
        return 1;
    }
    
    // Count tests that match filter
    size_t matching_tests = 0;
    for (const auto& test : tests) {
        if (matches_filter(test.name, filter)) {
            ++matching_tests;
        }
    }
    
    if (matching_tests == 0) {
        std::printf("No tests match filter: '%s'\n", filter ? filter : "");
        std::printf("Use --list to see all available tests\n");
        return 1;
    }
    
    // Run tests
    std::printf("========================================\n");
    std::printf("  Running Tests\n");
    std::printf("========================================\n");
    if (filter) {
        std::printf("Filter: %s\n", filter);
    }
    std::printf("Running %zu of %zu tests...\n\n", matching_tests, tests.size());
    
    size_t passed = 0;
    size_t failed = 0;
    size_t test_num = 0;
    
    for (const auto& test : tests) {
        if (!matches_filter(test.name, filter)) {
            continue;
        }
        
        ++test_num;
        std::printf("[%zu/%zu] %s... ", test_num, matching_tests, test.name);
        std::fflush(stdout);
        
        try {
            test.func();
            std::printf("✓ PASSED\n");
            ++passed;
        }
        catch (const AssertionFailure& e) {
            std::printf("✗ FAILED\n");
            std::printf("      %s\n", e.what());
            ++failed;
        }
        catch (const std::exception& e) {
            std::printf("✗ FAILED (exception)\n");
            std::printf("      %s\n", e.what());
            ++failed;
        }
        catch (...) {
            std::printf("✗ FAILED (unknown exception)\n");
            ++failed;
        }
    }
    
    // Print summary
    std::printf("\n========================================\n");
    std::printf("  Results\n");
    std::printf("========================================\n");
    std::printf("Passed: %zu\n", passed);
    std::printf("Failed: %zu\n", failed);
    std::printf("Total:  %zu\n", passed + failed);
    std::printf("========================================\n");
    
    return (failed == 0) ? 0 : 1;
}

} // namespace test_framework

/**
 * @def TEST(name)
 * @brief Define a test case.
 * 
 * The test name should be a valid C++ identifier. The test function
 * will be automatically registered and can be run via run_tests().
 * 
 * Example:
 * @code
 * TEST(addition_works) {
 *     TEST_ASSERT(1 + 1 == 2, "Basic addition");
 * }
 * @endcode
 */
#define TEST(name) \
    void test_##name(); \
    static test_framework::TestRegistrar test_registrar_##name(#name, test_##name); \
    void test_##name()

/**
 * @def TEST_ASSERT(condition, message)
 * @brief Assert that a condition is true.
 * 
 * If the condition is false, throws an AssertionFailure with the given message.
 * 
 * @param condition The condition to check
 * @param message Error message to display on failure
 */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            char buf[512]; \
            std::snprintf(buf, sizeof(buf), "%s:%d: Assertion failed: %s (%s)", \
                         __FILE__, __LINE__, #condition, message); \
            throw test_framework::AssertionFailure(buf); \
        } \
    } while (0)

/**
 * @def TEST_ASSERT_EQ(a, b, message)
 * @brief Assert that two values are equal.
 * 
 * @param a First value
 * @param b Second value
 * @param message Error message to display on failure
 */
#define TEST_ASSERT_EQ(a, b, message) \
    do { \
        auto val_a = (a); \
        auto val_b = (b); \
        if (!(val_a == val_b)) { \
            char buf[512]; \
            std::snprintf(buf, sizeof(buf), "%s:%d: Assertion failed: %s == %s (%s)", \
                         __FILE__, __LINE__, #a, #b, message); \
            throw test_framework::AssertionFailure(buf); \
        } \
    } while (0)

/**
 * @def TEST_ASSERT_NE(a, b, message)
 * @brief Assert that two values are not equal.
 * 
 * @param a First value
 * @param b Second value
 * @param message Error message to display on failure
 */
#define TEST_ASSERT_NE(a, b, message) \
    do { \
        auto val_a = (a); \
        auto val_b = (b); \
        if (!(val_a != val_b)) { \
            char buf[512]; \
            std::snprintf(buf, sizeof(buf), "%s:%d: Assertion failed: %s != %s (%s)", \
                         __FILE__, __LINE__, #a, #b, message); \
            throw test_framework::AssertionFailure(buf); \
        } \
    } while (0)

// Import run_tests into global namespace for convenience
using test_framework::run_tests;

