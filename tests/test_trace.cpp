/**
 * @file test_trace.cpp
 * @brief Basic functionality test for trace_scope library.
 * 
 * Tests:
 * - Multi-threaded tracing
 * - Binary dump creation
 * - File output verification
 */

#include "../include/trace_scope.hpp"
#include <thread>
#include <chrono>
#include <cassert>
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
int main() {
    TRACE_SCOPE();
    // trace::config.out = std::fopen("test_trace.log", "w");

    // Run branch() in separate thread and main thread
    std::thread t(branch);
    branch();
    t.join();

    // Flush all events to stdout
    trace::flush_all();
    
    // Create binary dump and verify success
    bool ok = trace::dump_binary("test_trace.bin");
    assert(ok && "dump_binary failed");

    // Verify binary file exists and has content
    struct stat st;
    assert(stat("test_trace.bin", &st) == 0 && "Binary file not created");
    assert(st.st_size > 0 && "Binary file is empty");

    // Basic sanity: ensure we can pretty print without crashing
    // (User can run tools/trc_pretty.py manually.)
    return 0;
}
