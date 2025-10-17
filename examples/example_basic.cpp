/**
 * @file example_basic.cpp
 * @brief Basic example demonstrating trace_scope usage with multiple threads.
 * 
 * Shows:
 * - Function entry/exit tracing with TRACE_SCOPE()
 * - Message logging with TRACE_MSG()
 * - Multi-threaded tracing
 * - Manual flushing and binary dump
 */

#include "../include/trace_scope.hpp"
#include <thread>
#include <chrono>
#include <cstdio>

/**
 * @brief Leaf function that performs work and logs messages.
 * @param i Iteration index
 */
void bar(int i) {
    TRACE_SCOPE();
    TRACE_MSG("bar start i=%d", i);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    TRACE_MSG("bar end i=%d", i);
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
        TRACE_MSG("t1 starting");
        foo();
        TRACE_MSG("t1 done");
        trace::flush_ring(trace::thread_ring());
    });

    // Do work on main thread
    foo();
    t1.join();

    // Flush all remaining events and create binary dump
    trace::flush_all();
    trace::dump_binary("trace.bin");
    
    return 0;
}
