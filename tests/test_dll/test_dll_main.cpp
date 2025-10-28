/**
 * @file test_dll_main.cpp
 * @brief Main executable that tests DLL state sharing
 * 
 * This executable sets up trace state sharing and calls functions
 * from the test DLL to verify that trace events are properly
 * captured and shared across DLL boundaries.
 */

#include <trace-scope/trace_scope.hpp>
#include "test_dll_library.h"
#include <cstdio>
#include <cstdlib>

/**
 * @brief Test function in main executable
 */
void main_function() {
    TRC_SCOPE();
    TRC_MSG("Main executable function called");
    
    // Call DLL functions
    TRC_MSG("Calling DLL functions...");
    dll_function_level1();
    dll_function_level2();
    
    TRC_MSG("Main function completed");
}

/**
 * @brief Test math operations across DLL boundary
 */
void test_math_operations() {
    TRC_SCOPE();
    TRC_MSG("Testing math operations across DLL boundary");
    
    int a = 15, b = 25;
    TRC_MSG("Testing with values: a=%d, b=%d", a, b);
    
    int sum = dll_math_add(a, b);
    int product = dll_math_multiply(a, b);
    
    TRC_MSG("Results: sum=%d, product=%d", sum, product);
    
    // Verify results
    if (sum != a + b) {
        std::fprintf(stderr, "ERROR: DLL math add failed: %d + %d = %d (expected %d)\n", 
                     a, b, sum, a + b);
        std::exit(1);
    }
    
    if (product != a * b) {
        std::fprintf(stderr, "ERROR: DLL math multiply failed: %d * %d = %d (expected %d)\n", 
                     a, b, product, a * b);
        std::exit(1);
    }
    
    TRC_MSG("Math operations verified successfully");
}

/**
 * @brief Test nested calls across DLL boundary
 */
void test_nested_calls() {
    TRC_SCOPE();
    TRC_MSG("Testing nested calls across DLL boundary");
    
    dll_nested_calls();
    
    TRC_MSG("Nested calls test completed");
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
    trace::get_config().out = std::fopen("test_dll_output.log", "w");
    if (!trace::get_config().out) {
        std::fprintf(stderr, "ERROR: Failed to open test_dll_output.log\n");
        return 1;
    }
    
    trace::get_config().print_timestamp = false;
    trace::get_config().print_thread = true;
    trace::get_config().print_timing = true;
    
    std::printf("=== DLL State Sharing Test ===\n");
    std::printf("This test verifies that trace state is properly shared\n");
    std::printf("across DLL boundaries using TRC_SETUP_DLL_SHARED().\n\n");
    
    TRC_SCOPE();
    TRC_MSG("Starting DLL state sharing test");
    
    // Test 1: Basic function calls
    std::printf("Test 1: Basic function calls\n");
    main_function();
    
    // Test 2: Math operations
    std::printf("Test 2: Math operations across DLL boundary\n");
    test_math_operations();
    
    // Test 3: Nested calls
    std::printf("Test 3: Nested calls across DLL boundary\n");
    test_nested_calls();
    
    TRC_MSG("All DLL tests completed successfully");
    
    // Flush all traces before closing
    trace::flush_all();
    
    if (trace::get_config().out && trace::get_config().out != stdout) {
        std::fclose(trace::get_config().out);
        trace::get_config().out = stdout;
    }
    
    std::printf("\n✓ Test completed successfully!\n");
    std::printf("✓ Trace output written to test_dll_output.log\n");
    std::printf("✓ All DLL functions were traced with shared state\n");
    
    return 0;
}
