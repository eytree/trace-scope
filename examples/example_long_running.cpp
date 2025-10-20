/**
 * @file example_long_running.cpp
 * @brief Example demonstrating periodic binary dumps with timestamped filenames.
 * 
 * Shows how repeated calls to dump_binary() create unique timestamped files,
 * preventing data loss from overwrites in long-running processes.
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <cstdio>

void do_work(int iteration) {
    TRACE_SCOPE();
    TRACE_MSG("Starting iteration %d", iteration);
    
    // Simulate some work
    for (int j = 0; j < 3; ++j) {
        TRACE_MSG("Work step %d/%d", j + 1, 3);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    TRACE_MSG("Completed iteration %d", iteration);
}

int main() {
    std::printf("=======================================================================\n");
    std::printf(" Long-Running Process with Periodic Binary Dumps\n");
    std::printf("=======================================================================\n\n");
    std::printf("This example demonstrates:\n");
    std::printf("  - Periodic dumps during long-running process\n");
    std::printf("  - Timestamped filenames prevent data loss\n");
    std::printf("  - Each dump creates a new file with unique timestamp\n");
    std::printf("  - Custom prefix for organizing trace files\n\n");
    std::printf("=======================================================================\n\n");
    
    // Configure custom prefix for this run
    trace::config.dump_prefix = "long_run";
    trace::config.mode = trace::TracingMode::Buffered;
    trace::config.out = nullptr;  // Don't print trace output
    
    TRACE_SCOPE();
    TRACE_MSG("Long-running process starting");
    
    std::printf("Simulating long-running process with 10 iterations...\n");
    std::printf("Dumping binary every 3 iterations:\n\n");
    
    for (int i = 0; i < 10; ++i) {
        do_work(i);
        
        // Dump periodically (every 3 iterations)
        if (i % 3 == 0) {
            // Each call creates a new timestamped file
            std::string filename = trace::dump_binary();
            if (!filename.empty()) {
                std::printf("  [Iteration %d] Dumped: %s\n", i, filename.c_str());
            }
            
            // Small delay to ensure different timestamps
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // Final dump
    std::string filename = trace::dump_binary();
    if (!filename.empty()) {
        std::printf("  [Final] Dumped: %s\n", filename.c_str());
    }
    
    TRACE_MSG("Long-running process complete");
    
    std::printf("\n=======================================================================\n");
    std::printf("✓ Complete - Generated multiple timestamped trace files\n");
    std::printf("\nAnalyze individual dumps:\n");
    std::printf("  python tools/trc_analyze.py display long_run_*.bin\n");
    std::printf("  python tools/trc_analyze.py stats long_run_*.bin\n");
    std::printf("\nBenefits:\n");
    std::printf("  - No data loss from overwrites\n");
    std::printf("  - Each dump is a snapshot in time\n");
    std::printf("  - Easy to track progression over time\n");
    std::printf("  - Files sorted chronologically by name\n");
    std::printf("=======================================================================\n");
    
    return 0;
}

