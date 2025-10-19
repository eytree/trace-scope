/**
 * @file example_thread_colors.cpp
 * @brief Demonstrates thread-aware color coding for multi-threaded tracing.
 * 
 * When colorize_depth is enabled, each thread gets a unique color offset based
 * on its thread ID. This makes it easy to visually distinguish which thread
 * produced each trace line, even when multiple threads trace at the same depth.
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <vector>
#include <chrono>

void recursive_function(int id, int depth) {
    TRACE_SCOPE();
    TRACE_MSG("Worker %d at depth %d", id, depth);
    
    if (depth > 0) {
        // Add small delay to interleave threads
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        recursive_function(id, depth - 1);
    }
}

void worker_thread(int id) {
    TRACE_SCOPE();
    TRACE_MSG("Worker %d starting", id);
    
    recursive_function(id, 3);
    
    TRACE_MSG("Worker %d done", id);
}

int main() {
    std::printf("=======================================================================\n");
    std::printf(" Thread-Aware Color Coding Demonstration\n");
    std::printf("=======================================================================\n");
    std::printf("\n");
    std::printf("This example demonstrates how thread-aware color coding makes\n");
    std::printf("multi-threaded traces easier to read.\n");
    std::printf("\n");
    std::printf("Each thread gets a unique color offset based on its thread ID:\n");
    std::printf("  - Thread 1: starts with Red, cycles through 8 colors\n");
    std::printf("  - Thread 2: starts with different color (offset by thread ID)\n");
    std::printf("  - Thread 3: starts with another color (different offset)\n");
    std::printf("\n");
    std::printf("Colors change with depth (nesting level), but each thread maintains\n");
    std::printf("its distinct color pattern throughout.\n");
    std::printf("\n");
    std::printf("=======================================================================\n");
    std::printf("\n");
    
    // Configure immediate mode with colors
    trace::config.mode = trace::TracingMode::Immediate;
    trace::config.colorize_depth = true;
    trace::config.print_timing = true;
    trace::config.print_thread = true;
    
    std::printf("Starting 3 worker threads...\n\n");
    
    {
        TRACE_SCOPE();
        TRACE_MSG("Main thread initializing workers");
        
        std::vector<std::thread> threads;
        
        // Launch 3 worker threads
        for (int i = 0; i < 3; ++i) {
            threads.emplace_back([i]() {
                worker_thread(i);
            });
        }
        
        // Wait for all threads to complete
        for (auto& t : threads) {
            t.join();
        }
        
        TRACE_MSG("All workers completed");
    }
    
    std::printf("\n");
    std::printf("=======================================================================\n");
    std::printf(" Summary\n");
    std::printf("=======================================================================\n");
    std::printf("\n");
    std::printf("Notice how:\n");
    std::printf("  1. Each thread uses a distinct set of colors\n");
    std::printf("  2. Colors cycle as depth increases (nested calls)\n");
    std::printf("  3. Easy to identify which thread produced each line\n");
    std::printf("  4. Thread IDs shown as [0xXXXX] match the color patterns\n");
    std::printf("\n");
    std::printf("This feature is automatic when colorize_depth = true!\n");
    std::printf("No configuration needed - colors assigned based on thread ID.\n");
    std::printf("\n");
    
    return 0;
}

