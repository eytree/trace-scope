/**
 * @file dll_main.cpp
 * @brief Example main executable that uses a traced DLL
 * 
 * This example demonstrates how to set up trace state sharing
 * between a main executable and a DLL using TRC_SETUP_DLL_SHARED().
 * All function calls across the DLL boundary will be traced
 * with unified output.
 */

#include <trace-scope/trace_scope.hpp>
#include "my_library.h"
#include <cstdio>
#include <vector>

/**
 * @brief Test function in main executable
 */
void main_test_function() {
    TRC_SCOPE();
    TRC_MSG("Main executable test function");
    
    // Call DLL functions
    TRC_MSG("Calling DLL math functions...");
    
    int test_value = 5;
    long long fact = factorial(test_value);
    long long fib = fibonacci(test_value);
    
    TRC_MSG("Results: %d! = %lld, F(%d) = %lld", test_value, fact, test_value, fib);
}

/**
 * @brief Test prime number checking
 */
void test_prime_numbers() {
    TRC_SCOPE();
    TRC_MSG("Testing prime number checking");
    
    std::vector<int> test_numbers = {2, 3, 4, 5, 17, 25, 29, 31, 100};
    
    for (int n : test_numbers) {
        TRC_MSG("Checking if %d is prime", n);
        bool prime = is_prime(n);
        TRC_MSG("%d is %s", n, prime ? "prime" : "not prime");
    }
}

/**
 * @brief Test GCD calculations
 */
void test_gcd_calculations() {
    TRC_SCOPE();
    TRC_MSG("Testing GCD calculations");
    
    std::vector<std::pair<int, int>> test_pairs = {
        {12, 18}, {48, 18}, {17, 13}, {100, 25}, {0, 5}
    };
    
    for (const auto& pair : test_pairs) {
        int a = pair.first, b = pair.second;
        TRC_MSG("Calculating GCD of %d and %d", a, b);
        int result = gcd(a, b);
        TRC_MSG("GCD(%d, %d) = %d", a, b, result);
    }
}

/**
 * @brief Test complex calculation
 */
void test_complex_calculation() {
    TRC_SCOPE();
    TRC_MSG("Testing complex calculation");
    
    int test_values[] = {3, 4, 5, 6};
    
    for (int n : test_values) {
        TRC_MSG("Running complex calculation for n=%d", n);
        long long result = complex_calculation(n);
        TRC_MSG("Complex calculation result for n=%d: %lld", n, result);
    }
}

/**
 * @brief Main function
 */
int main() {
    // ========================================================================
    // CRITICAL: Set up DLL state sharing BEFORE any tracing occurs
    // ========================================================================
    TRC_SETUP_DLL_SHARED();
    
    // Configure trace output
    trace::get_config().out = std::fopen("dll_example_output.log", "w");
    if (!trace::get_config().out) {
        std::fprintf(stderr, "ERROR: Failed to open dll_example_output.log\n");
        return 1;
    }
    
    trace::get_config().print_timestamp = false;
    trace::get_config().print_thread = true;
    trace::get_config().print_timing = true;
    
    std::printf("=== DLL State Sharing Example ===\n");
    std::printf("This example demonstrates trace state sharing between\n");
    std::printf("a main executable and a DLL using TRC_SETUP_DLL_SHARED().\n\n");
    
    TRC_SCOPE();
    TRC_MSG("Starting DLL example");
    
    // Test 1: Basic function calls
    std::printf("Test 1: Basic math functions\n");
    main_test_function();
    
    // Test 2: Prime number checking
    std::printf("Test 2: Prime number checking\n");
    test_prime_numbers();
    
    // Test 3: GCD calculations
    std::printf("Test 3: GCD calculations\n");
    test_gcd_calculations();
    
    // Test 4: Complex calculation
    std::printf("Test 4: Complex calculation\n");
    test_complex_calculation();
    
    TRC_MSG("DLL example completed successfully");
    
    // Flush all traces before closing
    trace::flush_all();
    
    if (trace::get_config().out && trace::get_config().out != stdout) {
        std::fclose(trace::get_config().out);
        trace::get_config().out = stdout;
    }
    
    std::printf("\n✓ Example completed successfully!\n");
    std::printf("✓ Trace output written to dll_example_output.log\n");
    std::printf("✓ All DLL functions were traced with shared state\n");
    std::printf("✓ Check the log file to see the unified trace output\n");
    
    return 0;
}
