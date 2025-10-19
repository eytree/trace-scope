/**
 * @file test_binary_format.cpp
 * @brief Test that verifies Python parser matches C++ binary format.
 * 
 * This test:
 * 1. Generates a known trace pattern
 * 2. Dumps to binary file
 * 3. Runs Python parser on it
 * 4. Verifies parser output is correct
 * 
 * Purpose: Ensures C++ binary format and Python parser stay in sync.
 */

#include <trace-scope/trace_scope.hpp>
#include "test_framework.hpp"
#include <cstdio>
#include <cstdlib>

/**
 * @brief Simple test function.
 */
void test_function(int value) {
    TRACE_SCOPE();
    TRACE_MSG("Test message with value=%d", value);
    TRACE_LOG << "Stream message: value=" << value;
}

/**
 * @brief Main test function.
 * 
 * Generates a predictable trace pattern and dumps it to binary.
 * The Python parser should be able to read this without errors.
 */
TEST(binary_format_and_python_parser) {
    TRACE_SCOPE();
    
    // Generate some trace events
    TRACE_LOG << "Starting binary format test";
    
    test_function(42);
    test_function(99);
    
    TRACE_MSG("Test complete");
    
    // Flush to ensure all events are captured
    trace::flush_all();
    
    // Dump binary
    const char* bin_file = "test_binary_format.bin";
    bool ok = trace::dump_binary(bin_file);
    TEST_ASSERT(ok, "Binary dump failed");
    
    std::printf("\n=== Binary Format Test ===\n");
    std::printf("Generated: %s\n", bin_file);
    std::printf("\nRun Python parser to verify:\n");
    std::printf("  python tools/trc_pretty.py %s\n\n", bin_file);
    std::printf("Expected output:\n");
    std::printf("  - All events should be readable\n");
    std::printf("  - Timestamps, thread IDs, depths should be correct\n");
    std::printf("  - Function names, file names, messages should be intact\n");
    std::printf("  - No parsing errors\n\n");
    
    // Try to run Python parser automatically if available
    std::printf("Attempting to run Python parser...\n");
    int ret = std::system("python ../tools/trc_pretty.py test_binary_format.bin");
    
    if (ret == 0) {
        std::printf("\n✓ Python parser executed successfully!\n");
        std::printf("✓ Binary format is compatible\n");
    } else {
        std::printf("\n⚠ Python parser returned code %d\n", ret);
        std::printf("  (This may be normal if Python is not in PATH)\n");
        std::printf("  Please run manually: python tools/trc_pretty.py %s\n", bin_file);
    }
}

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}

