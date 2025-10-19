/**
 * @file test_trace.cpp
 * @brief Basic functionality test for trace_scope library.
 * 
 * Tests:
 * - Multi-threaded tracing
 * - Binary dump creation
 * - File output verification
 */

#include <trace-scope/trace_scope.hpp>
#include "test_framework.hpp"
#include <thread>
#include <chrono>
#include <cstdio>
#include <sys/stat.h>

/**
 * @brief Leaf function for testing call depth.
 * @param n Test parameter
 */
static void leaf(int n) {
    TRACE_SCOPE();
    TRACE_MSG("leaf n=%d", n);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

/**
 * @brief Branch function that calls leaf multiple times.
 */
static void branch() {
    TRACE_SCOPE();
    for (int i = 0; i < 5; ++i) {
        leaf(i);
    }
}

/**
 * @brief Main test function.
 * 
 * Creates traces from multiple threads, flushes to binary file,
 * and verifies the output was created successfully.
 */
TEST(multi_threaded_binary_dump) {
    TRACE_SCOPE();
    
    // Run branch() in separate thread and main thread
    std::thread t(branch);
    branch();
    t.join();

    // Flush all events to stdout
    trace::flush_all();
    
    // Create binary dump and verify success
    bool ok = trace::dump_binary("test_trace.bin");
    TEST_ASSERT(ok, "dump_binary failed");

    // Verify binary file exists and has content
    struct stat st;
    TEST_ASSERT(stat("test_trace.bin", &st) == 0, "Binary file not created");
    TEST_ASSERT(st.st_size > 0, "Binary file is empty");
}

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
