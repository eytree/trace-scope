#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <vector>
#include <chrono>

/**
 * @brief Performance metrics demonstration example.
 * 
 * This example demonstrates the performance metrics system with:
 * - Multi-threaded execution with different workloads
 * - Memory tracking (RSS sampling)
 * - Automatic statistics at program exit
 * - Binary dump for Python analysis
 */

void fast_function() {
    TRACE_SCOPE();
    // Simulates fast operation - just some CPU work
    volatile int sum = 0;
    for (int i = 0; i < 1000; ++i) {
        sum += i;
    }
}

void slow_function() {
    TRACE_SCOPE();
    // Simulates slower operation with memory allocation
    std::vector<int> buffer(10000);
    for (size_t i = 0; i < buffer.size(); ++i) {
        buffer[i] = static_cast<int>(i);
    }
    // Simulate some processing time
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void memory_intensive_function() {
    TRACE_SCOPE();
    // Allocate significant memory to show memory tracking
    std::vector<std::vector<int>> big_arrays;
    for (int i = 0; i < 10; ++i) {
        big_arrays.emplace_back(50000);  // 50K ints = ~200KB per array
        for (size_t j = 0; j < big_arrays.back().size(); ++j) {
            big_arrays.back()[j] = static_cast<int>(j);
        }
    }
    // Keep arrays alive for a bit to show memory usage
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void worker_thread(int id) {
    TRACE_SCOPE();
    TRACE_MSG("Worker %d starting", id);
    
    // Mix of fast and slow operations
    for (int i = 0; i < 50; ++i) {
        if (i % 10 == 0) {
            slow_function();
        } else if (i % 5 == 0) {
            memory_intensive_function();
        } else {
            fast_function();
        }
    }
    
    TRACE_MSG("Worker %d completed", id);
}

int main() {
    std::printf("=======================================================================\n");
    std::printf(" Performance Metrics Demonstration\n");
    std::printf("=======================================================================\n\n");
    std::printf("This example demonstrates:\n");
    std::printf("  - Multi-threaded performance tracking\n");
    std::printf("  - Memory usage monitoring (RSS sampling)\n");
    std::printf("  - Automatic statistics at program exit\n");
    std::printf("  - Binary dump for Python analysis\n\n");
    std::printf("Configuration:\n");
    std::printf("  - print_stats = true (automatic exit statistics)\n");
    std::printf("  - track_memory = true (RSS sampling at each trace point)\n");
    std::printf("  - mode = Buffered (best performance)\n\n");
    std::printf("=======================================================================\n\n");
    
    // Configure for performance metrics
    trace::config.mode = trace::TracingMode::Buffered;
    trace::config.out = nullptr;  // Don't print trace output
    trace::config.print_stats = true;  // Print performance statistics at exit
    trace::config.track_memory = true;  // Sample RSS memory at each trace point
    
    // Ensure stats are registered for automatic exit
    trace::internal::ensure_stats_registered();
    
    TRACE_SCOPE();
    TRACE_MSG("Main thread initializing workers");
    
    // Create multiple worker threads with different workloads
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(worker_thread, i);
    }
    
    // Main thread also does some work
    for (int i = 0; i < 20; ++i) {
        if (i % 7 == 0) {
            memory_intensive_function();
        } else if (i % 3 == 0) {
            slow_function();
        } else {
            fast_function();
        }
    }
    
    // Wait for all workers to complete
    for (auto& t : threads) {
        t.join();
    }
    
    TRACE_MSG("All workers completed");
    
    // Dump binary trace for Python analysis
    std::string filename = trace::dump_binary("performance");
    if (!filename.empty()) {
        std::printf("✓ Binary trace saved to %s\n", filename.c_str());
        std::printf("  Use: python tools/trc_analyze.py stats %s\n", filename.c_str());
    }
    
    std::printf("\n✓ Program completed - statistics will be printed at exit\n");
    return 0;
}
