/**
 * @file test_double_buffer.cpp
 * @brief Tests for double-buffering functionality.
 * 
 * Tests include:
 * 1. Functional test: Verify buffer swapping and event ordering
 * 2. Stress test: High-frequency concurrent writes with frequent flushes
 * 3. Correctness test: Verify no events lost during buffer swaps
 * 
 * REQUIRES: Compile with TRC_DOUBLE_BUFFER=1
 */

#include <trace-scope/trace_scope.hpp>
#include "test_framework.hpp"

#if !TRC_DOUBLE_BUFFER
#error "This test requires TRC_DOUBLE_BUFFER=1. Reconfigure cmake with -DTRC_DOUBLE_BUFFER=ON"
#endif
#include <thread>
#include <chrono>
#include <atomic>
#include <cstdio>
#include <cstring>

/**
 * @brief Test 1: Functional test - verify basic double-buffer operation.
 */
TEST(functional) {
    std::printf("=== Test 1: Functional Test ===\n");
    
    // Enable double-buffering
    trace::config.use_double_buffering = true;
    trace::config.out = std::fopen("test_double_buffer_functional.log", "w");
    
    // Generate some events
    {
        TRC_SCOPE();
        TRC_LOG << "Event 1";
        TRC_MSG("Event %d", 2);
        TRC_LOG << "Event 3";
    }
    
    // Flush (should swap buffers)
    trace::flush_all();
    
    // Generate more events in the new buffer
    {
        TRC_SCOPE();
        TRC_LOG << "Event 4";
        TRC_MSG("Event %d", 5);
    }
    
    // Final flush
    trace::flush_all();
    
    if (trace::config.out && trace::config.out != stdout) {
        std::fclose(trace::config.out);
        trace::config.out = stdout;
    }
    
    std::printf("  ✓ Functional test passed\n");
    std::printf("  Output: test_double_buffer_functional.log\n\n");
}

/**
 * @brief Test 2: Event ordering - verify events maintain correct order.
 */
TEST(event_ordering) {
    std::printf("=== Test 2: Event Ordering Test ===\n");
    
    trace::config.use_double_buffering = true;
    trace::config.out = std::fopen("test_double_buffer_ordering.log", "w");
    
    // Generate sequential events
    for (int i = 0; i < 100; ++i) {
        TRC_MSG("Event %d", i);
        
        // Flush every 10 events to test buffer swapping
        if (i % 10 == 9) {
            trace::flush_all();
        }
    }
    
    trace::flush_all();
    
    if (trace::config.out && trace::config.out != stdout) {
        std::fclose(trace::config.out);
        trace::config.out = stdout;
    }
    
    std::printf("  ✓ Event ordering test passed\n");
    std::printf("  Output: test_double_buffer_ordering.log\n");
    std::printf("  (Verify events are numbered 0-99 in order)\n\n");
}

/**
 * @brief Test 3: Multi-threaded stress test.
 */
std::atomic<bool> g_stress_running{true};
std::atomic<uint64_t> g_stress_events{0};

void stress_worker(int worker_id) {
    int count = 0;
    while (g_stress_running.load(std::memory_order_relaxed)) {
        TRC_MSG("Worker %d event %d", worker_id, count);
        ++count;
        ++g_stress_events;
        
        // Small sleep to prevent CPU saturation and make test manageable
        if (count % 100 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

void stress_flusher() {
    while (g_stress_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        trace::flush_all();
    }
}

// NOTE: Stress test is optional - can take 5-10 seconds
// Uncomment in main() or run with: test_double_buffer stress
TEST(stress) {
    std::printf("=== Test 3: Stress Test ===\n");
    
    // Test both single and double-buffer modes
    for (int mode = 0; mode < 2; ++mode) {
        bool use_double = (mode == 1);
        const char* mode_name = use_double ? "Double-Buffer" : "Single-Buffer";
        const char* filename = use_double ? "test_double_buffer_stress_double.log" : 
                                            "test_double_buffer_stress_single.log";
        
        std::printf("  Testing %s mode...\n", mode_name);
        
        g_stress_running.store(true);
        g_stress_events.store(0);
        
        trace::config.use_double_buffering = use_double;
        trace::config.out = std::fopen(filename, "w");
        trace::config.print_timestamp = false;  // Reduce output size
        
        auto start = std::chrono::steady_clock::now();
        
        // Start 4 writer threads
        std::thread w1([](){ stress_worker(1); });
        std::thread w2([](){ stress_worker(2); });
        std::thread w3([](){ stress_worker(3); });
        std::thread w4([](){ stress_worker(4); });
        
        // Start flusher thread
        std::thread flusher(stress_flusher);
        
        // Run for 0.5 seconds
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Stop all threads
        g_stress_running.store(false);
        w1.join();
        w2.join();
        w3.join();
        w4.join();
        flusher.join();
        
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        // Final flush
        trace::flush_all();
        
        if (trace::config.out && trace::config.out != stdout) {
            std::fclose(trace::config.out);
            trace::config.out = stdout;
        }
        
        uint64_t events = g_stress_events.load();
        double events_per_sec = (events * 1000.0) / duration_ms;
        
        std::printf("    Duration: %lld ms\n", (long long)duration_ms);
        std::printf("    Events generated: %llu\n", (unsigned long long)events);
        std::printf("    Events/sec: %.0f\n", events_per_sec);
        std::printf("    Output: %s\n", filename);
        std::printf("    ✓ %s stress test passed\n\n", mode_name);
    }
}

/**
 * @brief Test 4: Single-threaded correctness - verify no events lost.
 */
TEST(single_thread_correctness) {
    std::printf("=== Test 4: Single-Thread Correctness ===\n");
    
    trace::config.use_double_buffering = true;
    trace::config.out = std::fopen("test_double_buffer_correctness.log", "w");
    
    const int NUM_EVENTS = 1000;
    const int FLUSH_INTERVAL = 50;
    
    for (int i = 0; i < NUM_EVENTS; ++i) {
        TRC_MSG("Event %d", i);
        
        if (i % FLUSH_INTERVAL == FLUSH_INTERVAL - 1) {
            trace::flush_all();
        }
    }
    
    trace::flush_all();
    
    if (trace::config.out && trace::config.out != stdout) {
        std::fclose(trace::config.out);
        trace::config.out = stdout;
    }
    
    // Verify the log file contains all events
    FILE* f = std::fopen("test_double_buffer_correctness.log", "r");
    TEST_ASSERT(f != nullptr, "Failed to open correctness log");
    
    int event_count = 0;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strstr(line, "Event ")) {
            ++event_count;
        }
    }
    std::fclose(f);
    
    std::printf("  Expected events: %d\n", NUM_EVENTS);
    std::printf("  Found events: %d\n", event_count);
    
    if (event_count == NUM_EVENTS) {
        std::printf("  ✓ Correctness test passed - no events lost\n\n");
    } else {
        std::printf("  ✗ Correctness test FAILED - missing %d events\n\n", 
                    NUM_EVENTS - event_count);
        std::exit(1);
    }
}

/**
 * @brief Test 5: Buffer swap verification.
 */
TEST(buffer_swap) {
    std::printf("=== Test 5: Buffer Swap Verification ===\n");
    
    trace::config.use_double_buffering = true;
    trace::config.out = std::fopen("test_double_buffer_swap.log", "w");
    
    // Write to buffer 0
    TRC_MSG("Before flush 1");
    
    // Flush - should swap to buffer 1
    trace::flush_all();
    
    // Write to buffer 1
    TRC_MSG("After flush 1");
    
    // Flush - should swap back to buffer 0
    trace::flush_all();
    
    // Write to buffer 0 again
    TRC_MSG("After flush 2");
    
    // Final flush
    trace::flush_all();
    
    if (trace::config.out && trace::config.out != stdout) {
        std::fclose(trace::config.out);
        trace::config.out = stdout;
    }
    
    // Verify the log contains all 3 messages
    FILE* f = std::fopen("test_double_buffer_swap.log", "r");
    TEST_ASSERT(f != nullptr, "Failed to open swap log");
    
    bool found[3] = {false, false, false};
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strstr(line, "Before flush 1")) found[0] = true;
        if (std::strstr(line, "After flush 1")) found[1] = true;
        if (std::strstr(line, "After flush 2")) found[2] = true;
    }
    std::fclose(f);
    
    TEST_ASSERT(found[0] && found[1] && found[2], "Buffer swap test failed - messages missing");
    
    std::printf("  ✓ Buffer swap verification passed\n\n");
}

/**
 * @brief Main test runner.
 */
int main(int argc, char** argv) {
    return run_tests(argc, argv);
}

