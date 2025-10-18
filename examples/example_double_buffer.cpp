/**
 * @file example_double_buffer.cpp
 * @brief Example demonstrating double-buffering for high-frequency tracing.
 * 
 * This example shows the benefits of double-buffering in scenarios with:
 * - High-frequency event generation (many events per millisecond)
 * - Frequent flush operations from separate threads
 * - Multiple concurrent writer threads
 * 
 * Double-buffering eliminates race conditions by alternating between two buffers:
 * - Write to buffer A while flushing buffer B
 * - After flush completes, swap: write to buffer B while flushing buffer A
 * 
 * This allows zero disruption during flush operations.
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdio>

std::atomic<bool> g_running{true};
std::atomic<uint64_t> g_event_count{0};

/**
 * @brief Fast function that generates many trace events.
 */
void fast_function(int id) {
    TRACE_SCOPE();
    TRACE_MSG("Fast event %d", id);
    ++g_event_count;
}

/**
 * @brief Worker thread that generates high-frequency events.
 */
void high_frequency_worker(int worker_id) {
    TRACE_SCOPE();
    TRACE_LOG << "Worker " << worker_id << " starting";
    
    int event_id = 0;
    while (g_running.load(std::memory_order_relaxed)) {
        fast_function(worker_id * 10000 + event_id);
        ++event_id;
        
        // Optional: small delay to prevent CPU saturation (comment out for max speed)
        // std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    
    TRACE_LOG << "Worker " << worker_id << " done, generated " << event_id << " events";
}

/**
 * @brief Flusher thread that periodically flushes all trace buffers.
 */
void periodic_flusher() {
    TRACE_SCOPE();
    TRACE_LOG << "Flusher starting";
    
    int flush_count = 0;
    while (g_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Flush all ring buffers
        // In double-buffer mode: this swaps buffers and flushes old buffer
        // In single-buffer mode: this may have race conditions with concurrent writes
        trace::flush_all();
        ++flush_count;
    }
    
    TRACE_LOG << "Flusher done, performed " << flush_count << " flushes";
}

/**
 * @brief Run stress test with specified buffer mode.
 */
void run_stress_test(bool use_double_buffer, const char* output_file) {
    g_running.store(true);
    g_event_count.store(0);
    
    // Configure tracing
    trace::config.use_double_buffering = use_double_buffer;
    trace::config.out = std::fopen(output_file, "w");
    trace::config.print_timestamp = false;  // Reduce output size for high-frequency
    
    std::printf("\n=== Stress Test: %s ===\n", 
                use_double_buffer ? "Double-Buffer Mode" : "Single-Buffer Mode");
    std::printf("Configuration:\n");
    std::printf("  - Output file: %s\n", output_file);
    std::printf("  - Buffer mode: %s\n", use_double_buffer ? "DOUBLE" : "SINGLE");
    std::printf("  - Worker threads: 4\n");
    std::printf("  - Flush interval: 50ms\n");
    std::printf("  - Test duration: 2 seconds\n\n");
    
    TRACE_SCOPE();
    TRACE_LOG << "Starting stress test";
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Start worker threads (high-frequency event generators)
    std::thread w1([](){ high_frequency_worker(1); });
    std::thread w2([](){ high_frequency_worker(2); });
    std::thread w3([](){ high_frequency_worker(3); });
    std::thread w4([](){ high_frequency_worker(4); });
    
    // Start flusher thread (periodic flush operations)
    std::thread flusher(periodic_flusher);
    
    // Run for 1 second (reduced for faster testing)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Stop all threads
    g_running.store(false);
    w1.join();
    w2.join();
    w3.join();
    w4.join();
    flusher.join();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    // Final flush
    trace::flush_all();
    
    TRACE_LOG << "Stress test complete";
    
    // Print statistics
    uint64_t total_events = g_event_count.load();
    double events_per_sec = (total_events * 1000.0) / duration_ms;
    
    std::printf("Results:\n");
    std::printf("  - Duration: %lld ms\n", (long long)duration_ms);
    std::printf("  - Total events: %llu\n", (unsigned long long)total_events);
    std::printf("  - Events/sec: %.0f\n", events_per_sec);
    std::printf("  - Output saved to: %s\n", output_file);
    
    if (trace::config.out && trace::config.out != stdout) {
        std::fclose(trace::config.out);
        trace::config.out = stdout;
    }
}

/**
 * @brief Main function demonstrating double-buffering benefits.
 */
int main() {
    std::printf("=== Double-Buffering Stress Test ===\n\n");
    std::printf("This example demonstrates double-buffering for high-frequency tracing.\n");
    std::printf("We'll run the same stress test twice:\n");
    std::printf("  1. Single-buffer mode (may have race conditions)\n");
    std::printf("  2. Double-buffer mode (race-free, safe concurrent flush)\n\n");
    
    // Test 1: Single-buffer mode
    run_stress_test(false, "stress_single_buffer.log");
    
    // Small pause between tests
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Test 2: Double-buffer mode
    run_stress_test(true, "stress_double_buffer.log");
    
    std::printf("\n=== Tests Complete ===\n");
    std::printf("\nKey observations:\n");
    std::printf("  - Double-buffer mode provides race-free flush operations\n");
    std::printf("  - Writers continue unblocked during flush\n");
    std::printf("  - Memory usage: 2x per thread in double-buffer mode\n");
    std::printf("  - Both modes should produce similar event counts\n");
    std::printf("\nCheck the output files:\n");
    std::printf("  - stress_single_buffer.log\n");
    std::printf("  - stress_double_buffer.log\n");
    
    return 0;
}

