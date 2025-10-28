/**
 * @file example_basic.cpp
 * @brief Basic example demonstrating trace_scope usage with multiple threads.
 * 
 * Shows:
 * - Function entry/exit tracing with TRC_SCOPE()
 * - Printf-style logging with TRC_MSG()
 * - Stream-based logging with TRC_LOG (drop-in for stream macros)
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
 * Demonstrates both printf-style (TRC_MSG) and stream-style (TRC_LOG) logging.
 * 
 * @param i Iteration index
 */
void bar(int i) {
    TRC_SCOPE();
    TRC_LOG << "bar start i=" << i;  // Stream-based logging
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    TRC_MSG("bar end i=%d", i);  // Printf-style logging
}

/**
 * @brief Mid-level function that calls bar multiple times.
 */
void foo() {
    TRC_SCOPE();
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
    TRC_SCOPE();
    
    // Configure output to file instead of stdout
    trace::config.out = trace::safe_fopen("trace.log", "w");

    // Create worker thread
    std::thread t1([]() {
        TRC_SCOPE();
        TRC_LOG << "t1 starting";  // Stream-based
        foo();
        TRC_LOG << "t1 done";  // Stream-based
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
