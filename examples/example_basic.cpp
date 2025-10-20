/**
 * @file example_basic.cpp
 * @brief Basic example demonstrating trace_scope usage with multiple threads.
 * 
 * Shows:
 * - Function entry/exit tracing with TRACE_SCOPE()
 * - Printf-style logging with TRACE_MSG()
 * - Stream-based logging with TRACE_LOG (drop-in for stream macros)
 * - Logging function arguments
 * - Multi-threaded tracing
 * - Manual flushing and binary dump
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <cstdio>

/**
 * @brief Leaf function that performs work and logs messages.
 * 
 * Demonstrates both printf-style (TRACE_MSG) and stream-style (TRACE_LOG) logging.
 * 
 * @param i Iteration index
 */
void bar(int i) {
    TRACE_SCOPE();
    TRACE_LOG << "bar start i=" << i;  // Stream-based logging
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    TRACE_MSG("bar end i=%d", i);  // Printf-style logging
}

/**
 * @brief Mid-level function that calls bar multiple times.
 */
void foo() {
    TRACE_SCOPE();
    for (int i = 0; i < 3; ++i) {
        bar(i);
    }
}

/**
 * @brief Main function demonstrating multi-threaded tracing.
 * 
 * Creates a worker thread that runs foo() while the main thread
 * also runs foo(). All events are collected and flushed at the end.
 */
int main() {
    TRACE_SCOPE();
    
    // Configure output to file instead of stdout
    trace::config.out = std::fopen("trace.log", "w");

    // Create worker thread
    std::thread t1([]() {
        TRACE_SCOPE();
        TRACE_LOG << "t1 starting";  // Stream-based
        foo();
        TRACE_LOG << "t1 done";  // Stream-based
        trace::flush_ring(trace::thread_ring());
    });

    // Do work on main thread
    foo();
    t1.join();

    // Flush all remaining events and create binary dump
    trace::flush_all();
    
    std::string filename = trace::dump_binary();
    if (!filename.empty()) {
        std::printf("Binary trace saved to %s\n", filename.c_str());
    }
    
    return 0;
}
