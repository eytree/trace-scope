/**
 * @file test_async_immediate.cpp
 * @brief Tests for async immediate mode
 */

#include "test_framework.hpp"
#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

TEST(basic_async_immediate) {
    // Test basic async immediate mode - events should appear in output
    trace::config.out = std::fopen("test_async_basic.log", "w");
    trace::config.mode = trace::TracingMode::Immediate;
    
    {
        TRC_SCOPE();
        TRC_MSG("Test message 1");
        TRC_MSG("Test message 2");
    }
    
    // Force flush to ensure events written
    trace::flush_immediate_queue();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
    
    // Verify output file exists and has content
    std::ifstream in("test_async_basic.log");
    TEST_ASSERT(in.good(), "Output file should exist");
    
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    TEST_ASSERT(content.find("basic_async_immediate") != std::string::npos, 
                "Output should contain function name");
    TEST_ASSERT(content.find("Test message 1") != std::string::npos, 
                "Output should contain message 1");
    TEST_ASSERT(content.find("Test message 2") != std::string::npos, 
                "Output should contain message 2");
}

TEST(multi_threaded_async_immediate) {
    // Test that async immediate mode works correctly with multiple threads
    trace::config.out = std::fopen("test_async_multithread.log", "w");
    trace::config.mode = trace::TracingMode::Immediate;
    
    const int num_threads = 4;
    const int iterations = 10;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t, iterations]() {
            for (int i = 0; i < iterations; ++i) {
                TRC_SCOPE();
                TRC_MSG("Thread %d iteration %d", t, i);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    // Force flush and wait for queue to drain
    trace::flush_immediate_queue();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
    
    // Verify output
    std::ifstream in("test_async_multithread.log");
    TEST_ASSERT(in.good(), "Output file should exist");
    
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    
    // Check that all threads produced output
    for (int t = 0; t < num_threads; ++t) {
        std::string needle = "Thread " + std::to_string(t);
        TEST_ASSERT(content.find(needle) != std::string::npos, 
                    "Output should contain events from all threads");
    }
}

TEST(flush_immediate_queue_blocks) {
    // Test that flush_immediate_queue() actually blocks until queue is drained
    trace::config.out = std::fopen("test_async_flush.log", "w");
    trace::config.mode = trace::TracingMode::Immediate;
    
    // Generate events
    for (int i = 0; i < 50; ++i) {
        TRC_MSG("Event %d", i);
    }
    
    // Flush should block until all events written
    auto start = std::chrono::steady_clock::now();
    trace::flush_immediate_queue();
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    // Should have waited some time (not instant)
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    TEST_ASSERT(elapsed_us < 1000000, "flush_immediate_queue() should not timeout (1s)");
    
    std::fclose(trace::config.out);
    trace::config.out = stdout;
    
    // Verify all events written
    std::ifstream in("test_async_flush.log");
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    
    TEST_ASSERT(content.find("Event 0") != std::string::npos, "First event written");
    TEST_ASSERT(content.find("Event 49") != std::string::npos, "Last event written");
}

TEST(async_queue_atexit_handler) {
    // Test that atexit handler properly flushes queue on shutdown
    // This is implicitly tested by other tests, but verify explicitly
    trace::config.out = std::fopen("test_async_atexit.log", "w");
    trace::config.mode = trace::TracingMode::Immediate;
    
    {
        TRC_SCOPE();
        TRC_MSG("Before shutdown");
    }
    
    // Manually stop async queue (simulates atexit)
    trace::stop_async_immediate();
    
    std::fclose(trace::config.out);
    trace::config.out = stdout;
    
    // Verify events were written
    std::ifstream in("test_async_atexit.log");
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    
    TEST_ASSERT(content.find("async_queue_atexit_handler") != std::string::npos,
                "Function name should be in output");
    TEST_ASSERT(content.find("Before shutdown") != std::string::npos,
                "Message should be in output");
}

TEST(configurable_flush_interval) {
    // Test different flush intervals
    trace::config.out = std::fopen("test_async_interval.log", "w");
    trace::config.mode = trace::TracingMode::Immediate;
    trace::config.immediate_flush_interval_ms = 10;  // 10ms interval
    
    // Start async mode with custom config
    trace::start_async_immediate();
    
    TRC_MSG("Event with 10ms interval");
    
    // Wait a bit longer than interval
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    trace::flush_immediate_queue();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
    
    // Verify output
    std::ifstream in("test_async_interval.log");
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    
    TEST_ASSERT(content.find("Event with 10ms interval") != std::string::npos,
                "Event should be written");
    
    // Reset for next test
    trace::stop_async_immediate();
    trace::config.immediate_flush_interval_ms = 1;  // Reset to default
}

TEST(hybrid_mode_with_async) {
    // Test that hybrid mode uses async queue for immediate output
    trace::config.out = std::fopen("test_async_hybrid_buffered.log", "w");
    trace::config.immediate_out = std::fopen("test_async_hybrid_immediate.log", "w");
    trace::config.mode = trace::TracingMode::Hybrid;
    
    {
        TRC_SCOPE();
        TRC_MSG("Hybrid mode message");
    }
    
    // Flush both buffered and immediate outputs
    trace::flush_all();
    trace::flush_immediate_queue();
    
    std::fclose(trace::config.out);
    std::fclose(trace::config.immediate_out);
    trace::config.out = stdout;
    trace::config.immediate_out = nullptr;
    
    // Verify both outputs have content
    std::ifstream buffered("test_async_hybrid_buffered.log");
    std::string buffered_content((std::istreambuf_iterator<char>(buffered)),
                                  std::istreambuf_iterator<char>());
    
    std::ifstream immediate("test_async_hybrid_immediate.log");
    std::string immediate_content((std::istreambuf_iterator<char>(immediate)),
                                   std::istreambuf_iterator<char>());
    
    TEST_ASSERT(buffered_content.find("hybrid_mode_with_async") != std::string::npos,
                "Buffered output should have function name");
    TEST_ASSERT(immediate_content.find("hybrid_mode_with_async") != std::string::npos,
                "Immediate output should have function name");
    TEST_ASSERT(immediate_content.find("Hybrid mode message") != std::string::npos,
                "Immediate output should have message");
    
    // Reset
    trace::stop_async_immediate();
    trace::config.mode = trace::TracingMode::Buffered;
}

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}

