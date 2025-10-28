/**
 * @file example_async_benchmark.cpp
 * @brief Performance benchmark comparing async immediate mode
 * 
 * Measures overhead per trace call with different configurations:
 * - Buffered mode (baseline)
 * - Async immediate mode (v0.9.0+)
 * - Multi-threaded scaling
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdio>

// Benchmark configuration
const int WARMUP_ITERATIONS = 1000;
const int BENCH_ITERATIONS = 10000;

void benchmark_function() {
    TRC_SCOPE();
    // Empty function - just measure trace overhead
}

double measure_overhead(trace::TracingMode mode, int num_threads = 1) {
    // Configure mode
    trace::config.mode = mode;
    
    if (mode == trace::TracingMode::Immediate) {
        // Restart async queue for immediate mode
        trace::stop_async_immediate();
        trace::start_async_immediate(trace::safe_fopen("benchmark_immediate.log", "w"));
    } else {
        trace::config.out = trace::safe_fopen("benchmark_buffered.log", "w");
    }
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        benchmark_function();
    }
    
    if (mode == trace::TracingMode::Immediate) {
        trace::flush_immediate_queue();
    } else {
        trace::flush_all();
    }
    
    // Actual benchmark
    auto start = std::chrono::high_resolution_clock::now();
    
    if (num_threads == 1) {
        // Single-threaded
        for (int i = 0; i < BENCH_ITERATIONS; ++i) {
            benchmark_function();
        }
    } else {
        // Multi-threaded
        std::vector<std::thread> threads;
        int per_thread = BENCH_ITERATIONS / num_threads;
        
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([per_thread]() {
                for (int i = 0; i < per_thread; ++i) {
                    benchmark_function();
                }
            });
        }
        
        for (auto& th : threads) {
            th.join();
        }
    }
    
    if (mode == trace::TracingMode::Immediate) {
        trace::flush_immediate_queue();
    } else {
        trace::flush_all();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    
    // Calculate overhead per trace call (each function = 2 events: enter + exit)
    double overhead_ns = (double)elapsed_ns / (BENCH_ITERATIONS * 2);
    
    // Cleanup
    if (mode == trace::TracingMode::Immediate) {
        trace::stop_async_immediate();
        if (trace::config.out && trace::config.out != stdout) {
            std::fclose(trace::config.out);
        }
    } else {
        if (trace::config.out && trace::config.out != stdout) {
            std::fclose(trace::config.out);
        }
    }
    trace::config.out = stdout;
    
    return overhead_ns;
}

int main() {
    std::printf("=====================================================\n");
    std::printf("Async Immediate Mode Performance Benchmark\n");
    std::printf("trace-scope v%s\n", TRC_SCOPE_VERSION);
    std::printf("=====================================================\n\n");
    
    std::printf("Benchmark configuration:\n");
    std::printf("  Iterations: %d\n", BENCH_ITERATIONS);
    std::printf("  Events per iteration: 2 (enter + exit)\n");
    std::printf("  Total events: %d\n\n", BENCH_ITERATIONS * 2);
    
    // Single-threaded benchmarks
    std::printf("Single-Threaded Performance:\n");
    std::printf("-----------------------------------------------------\n");
    
    double buffered_overhead = measure_overhead(trace::TracingMode::Buffered, 1);
    std::printf("  Buffered mode:        %8.2f ns/trace\n", buffered_overhead);
    
    double async_overhead = measure_overhead(trace::TracingMode::Immediate, 1);
    std::printf("  Async Immediate mode: %8.2f ns/trace\n", async_overhead);
    
    double speedup = buffered_overhead / async_overhead;
    if (async_overhead > buffered_overhead) {
        double overhead_ratio = async_overhead / buffered_overhead;
        std::printf("  Overhead vs buffered: %.1fx slower\n", overhead_ratio);
    } else {
        std::printf("  Speedup vs buffered:  %.1fx faster\n", speedup);
    }
    std::printf("\n");
    
    // Multi-threaded benchmarks
    std::printf("Multi-Threaded Performance (4 threads):\n");
    std::printf("-----------------------------------------------------\n");
    
    double buffered_mt = measure_overhead(trace::TracingMode::Buffered, 4);
    std::printf("  Buffered mode:        %8.2f ns/trace\n", buffered_mt);
    
    double async_mt = measure_overhead(trace::TracingMode::Immediate, 4);
    std::printf("  Async Immediate mode: %8.2f ns/trace\n", async_mt);
    
    double mt_speedup = buffered_mt / async_mt;
    if (async_mt > buffered_mt) {
        double overhead_ratio = async_mt / buffered_mt;
        std::printf("  Overhead vs buffered: %.1fx slower\n", overhead_ratio);
    } else {
        std::printf("  Speedup vs buffered:  %.1fx faster\n", mt_speedup);
    }
    std::printf("\n");
    
    // Analysis
    std::printf("=====================================================\n");
    std::printf("Analysis:\n");
    std::printf("=====================================================\n\n");
    
    std::printf("Async immediate mode provides:\n");
    std::printf("  • Non-blocking writes - threads don't wait for I/O\n");
    std::printf("  • Batched flushing - better I/O throughput\n");
    std::printf("  • No mutex contention on hot path\n");
    std::printf("  • Real-time output with minimal latency (~1ms)\n\n");
    
    std::printf("Expected results:\n");
    std::printf("  • Async overhead: ~100-500ns (queue insertion)\n");
    std::printf("  • Similar to buffered mode overhead\n");
    std::printf("  • Better multi-thread scaling than synchronous\n\n");
    
    std::printf("Use flush_immediate_queue() when you need guarantees:\n");
    std::printf("  trace::flush_immediate_queue();  // Blocks until queue drained\n\n");
    
    return 0;
}

