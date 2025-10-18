/**
 * @file example_hybrid.cpp
 * @brief Example demonstrating hybrid buffered+immediate mode.
 * 
 * Hybrid mode combines the benefits of both buffered and immediate modes:
 * - Real-time visibility (see events as they happen)
 * - Complete history (buffered for post-processing)
 * - Auto-flush when buffer nears capacity (prevents data loss)
 * 
 * Use case: Long-running processes where you want both real-time monitoring
 * and the ability to analyze the complete trace history later.
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <cstdio>

/**
 * @brief Simulates work with logging.
 */
void do_work(int item_id) {
    TRACE_SCOPE();
    TRACE_LOG << "Processing item " << item_id;
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    TRACE_MSG("Item %d complete", item_id);
}

/**
 * @brief Worker function that processes multiple items.
 */
void worker_thread(int worker_id, int item_count) {
    TRACE_SCOPE();
    TRACE_LOG << "Worker " << worker_id << " starting, processing " << item_count << " items";
    
    for (int i = 0; i < item_count; ++i) {
        do_work(worker_id * 100 + i);
    }
    
    TRACE_LOG << "Worker " << worker_id << " done";
}

/**
 * @brief Main function demonstrating hybrid mode.
 */
int main() {
    TRACE_SCOPE();
    
    std::printf("=== Hybrid Mode Demo ===\n\n");
    std::printf("Hybrid mode provides:\n");
    std::printf("  1. Real-time output (see trace as it happens)\n");
    std::printf("  2. Buffered history (for post-processing)\n");
    std::printf("  3. Auto-flush when buffer nears 90%% capacity\n\n");
    
    // Configure hybrid mode
    trace::config.hybrid_mode = true;
    trace::config.auto_flush_threshold = 0.8f;  // Flush at 80% full
    
    // Real-time output goes to console (immediate visibility)
    trace::config.immediate_out = stdout;
    
    // Buffered output saved to file (complete history for post-processing)
    trace::config.out = std::fopen("hybrid_buffered.log", "w");
    
    std::printf("Starting simulation with %d items across 3 threads...\n", 30);
    std::printf("Watch this output for real-time progress!\n\n");
    
    TRACE_LOG << "Simulation starting";
    
    // Create worker threads
    std::thread t1([](){ worker_thread(1, 10); });
    std::thread t2([](){ worker_thread(2, 10); });
    std::thread t3([](){ worker_thread(3, 10); });
    
    // Wait for all workers to complete
    t1.join();
    t2.join();
    t3.join();
    
    TRACE_LOG << "All workers complete";
    
    // Final flush to capture any remaining buffered events
    trace::flush_all();
    
    std::printf("\n=== Simulation Complete ===\n");
    std::printf("Immediate output was shown above in real-time.\n");
    std::printf("Complete trace history saved to: hybrid_buffered.log\n");
    std::printf("Note: Auto-flush triggered automatically when buffer reached 80%% full\n");
    
    // Optional: also dump binary for analysis
    trace::dump_binary("hybrid.bin");
    std::printf("Binary dump: hybrid.bin (use trc_pretty.py to view)\n");
    
    if (trace::config.out && trace::config.out != stdout) {
        std::fclose(trace::config.out);
    }
    
    return 0;
}


