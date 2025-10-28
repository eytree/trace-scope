/**
 * @file example_async_immediate.cpp
 * @brief Demonstrates async immediate mode with background writer thread
 * 
 * Shows:
 * - Basic async immediate mode usage
 * - flush_immediate_queue() for synchronous semantics when needed
 * - Configurable flush intervals
 * - Multi-threaded async immediate tracing
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <vector>

void worker_task(int id, int iterations) {
    TRC_SCOPE();
    for (int i = 0; i < iterations; ++i) {
        TRC_MSG("Worker %d: iteration %d", id, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void critical_section() {
    TRC_SCOPE();
    TRC_MSG("Before critical operation");
    
    // Critical operation
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    TRC_MSG("After critical operation");
    
    // Force flush before proceeding (ensures events written if we crash next)
    trace::flush_immediate_queue();
    
    TRC_MSG("Critical section complete - events guaranteed written");
}

int main() {
    std::printf("=================================================\n");
    std::printf("Async Immediate Mode Example (v%s)\n", TRC_SCOPE_VERSION);
    std::printf("=================================================\n\n");
    
    // Configure for immediate mode
    trace::config.out = trace::safe_fopen("async_immediate.log", "w");
    trace::config.mode = trace::TracingMode::Immediate;
    trace::config.immediate_flush_interval_ms = 1;  // Flush every 1ms (default)
    
    std::printf("Test 1: Basic async immediate mode\n");
    std::printf("--------------------------------------------------\n");
    std::printf("Events written asynchronously with ~1ms latency\n");
    std::printf("Output file: async_immediate.log\n\n");
    
    {
        TRC_SCOPE();
        TRC_MSG("Starting basic test");
        
        for (int i = 0; i < 5; ++i) {
            TRC_MSG("Loop iteration %d", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        TRC_MSG("Basic test complete");
    }
    
    std::printf("✓ Basic test complete\n\n");
    
    // Test 2: Critical section with forced flush
    std::printf("Test 2: Critical section with flush_immediate_queue()\n");
    std::printf("--------------------------------------------------\n");
    std::printf("Demonstrates forcing synchronous flush when needed\n\n");
    
    critical_section();
    
    std::printf("✓ Critical section complete\n\n");
    
    // Test 3: Multi-threaded async immediate
    std::printf("Test 3: Multi-threaded async immediate mode\n");
    std::printf("--------------------------------------------------\n");
    std::printf("Multiple threads trace concurrently without blocking\n\n");
    
    {
        TRC_SCOPE();
        std::vector<std::thread> threads;
        
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back(worker_task, t, 3);
        }
        
        for (auto& th : threads) {
            th.join();
        }
        
        TRC_MSG("All worker threads completed");
    }
    
    std::printf("✓ Multi-threaded test complete\n\n");
    
    // Test 4: Custom flush interval
    std::printf("Test 4: Custom flush interval (10ms)\n");
    std::printf("--------------------------------------------------\n");
    std::printf("Larger intervals improve throughput, add latency\n\n");
    
    // Stop current async queue
    trace::stop_async_immediate();
    
    // Restart with longer interval
    trace::config.immediate_flush_interval_ms = 10;
    trace::start_async_immediate();
    
    {
        TRC_SCOPE();
        for (int i = 0; i < 20; ++i) {
            TRC_MSG("Fast event %d", i);
        }
        
        // Events batched and written every 10ms
        TRC_MSG("20 events batched for efficiency");
    }
    
    // Final flush
    trace::flush_immediate_queue();
    
    std::printf("✓ Custom interval test complete\n\n");
    
    // Cleanup
    trace::stop_async_immediate();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
    
    std::printf("=================================================\n");
    std::printf("All tests completed!\n");
    std::printf("=================================================\n\n");
    std::printf("Key Benefits of Async Immediate Mode:\n");
    std::printf("  • 100x lower overhead vs synchronous (~1µs vs ~100µs)\n");
    std::printf("  • Non-blocking - traced threads don't wait for I/O\n");
    std::printf("  • Better multi-threading - no mutex contention\n");
    std::printf("  • Still real-time - events appear within milliseconds\n");
    std::printf("  • Batched I/O - better throughput\n\n");
    std::printf("Use flush_immediate_queue() when you need synchronous guarantees:\n");
    std::printf("  • Before critical operations that might crash\n");
    std::printf("  • In test code to verify output\n");
    std::printf("  • When switching output files\n\n");
    
    return 0;
}

