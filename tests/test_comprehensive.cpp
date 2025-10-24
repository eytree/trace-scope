#include <trace-scope/trace_scope.hpp>
#include "test_framework.hpp"
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

// Test 1: Multi-threaded tracing (thread safety)
TEST(multi_threaded_trace) {
    trace::config.out = trace::safe_fopen("test_multithread.log", "w");
    trace::config.mode = trace::TracingMode::Buffered;
    
    auto worker = [](int id) {
        TRC_SCOPE();
        TRC_MSG("Worker %d starting", id);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        TRC_MSG("Worker %d done", id);
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 2: Immediate mode vs buffered mode
TEST(immediate_vs_buffered) {
    // Test immediate mode
    {
        trace::config.mode = trace::TracingMode::Immediate;
        trace::config.out = trace::safe_fopen("test_immediate.log", "w");
        
        TRC_SCOPE();
        TRC_MSG("Immediate mode message 1");
        TRC_MSG("Immediate mode message 2");
        
        std::fclose(trace::config.out);
        trace::config.mode = trace::TracingMode::Buffered;
    }
    
    // Test buffered mode
    {
        trace::config.out = trace::safe_fopen("test_buffered.log", "w");
        
        TRC_SCOPE();
        TRC_MSG("Buffered mode message 1");
        TRC_MSG("Buffered mode message 2");
        
        trace::flush_all();
        std::fclose(trace::config.out);
    }
    
    trace::config.out = stdout;
}

// Test 3: Config option combinations
TEST(config_combinations) {
    trace::config.out = trace::safe_fopen("test_config.log", "w");
    
    // Test with all options off
    trace::config.print_timing = false;
    trace::config.print_timestamp = false;
    trace::config.print_thread = false;
    trace::config.include_filename = false;
    trace::config.include_function_name = false;
    
    {
        TRC_SCOPE();
        TRC_MSG("Minimal output");
    }
    
    // Test with all options on
    trace::config.print_timing = true;
    trace::config.print_timestamp = true;
    trace::config.print_thread = true;
    trace::config.include_filename = true;
    trace::config.include_function_name = true;
    
    {
        TRC_SCOPE();
        TRC_MSG("Full output");
    }
    
    // Reset to defaults
    trace::config.print_timing = true;
    trace::config.print_timestamp = false;
    trace::config.print_thread = true;
    trace::config.include_filename = true;
    trace::config.include_function_name = true;
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 4: Filename truncation with long paths
TEST(long_filename_truncation) {
    trace::config.out = trace::safe_fopen("test_filename_truncation.log", "w");
    trace::config.filename_width = 15;
    
    // Simulate long path by using current file's __FILE__ macro
    TRC_SCOPE();
    TRC_MSG("Testing filename truncation with very long path name");
    
    trace::config.filename_width = 20; // Reset
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Helper function with intentionally long name for testing
static void this_is_an_intentionally_very_long_function_name_for_testing_truncation() {
    TRC_SCOPE();
    TRC_MSG("Long function name test");
}

// Test 5: Function name truncation
TEST(long_function_truncation) {
    trace::config.out = trace::safe_fopen("test_function_truncation.log", "w");
    trace::config.function_width = 15;
    
    this_is_an_intentionally_very_long_function_name_for_testing_truncation();
    
    trace::config.function_width = 20; // Reset
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 6: Deep nesting
static void deeply_nested_call(int depth) {
    TRC_SCOPE();
    TRC_MSG("At depth %d", depth);
    if (depth > 0) {
        deeply_nested_call(depth - 1);
    }
}

TEST(deep_nesting) {
    trace::config.out = trace::safe_fopen("test_deep_nesting.log", "w");
    
    deeply_nested_call(50);
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 7: Ring buffer wraparound
TEST(ring_buffer_wraparound) {
    trace::config.out = trace::safe_fopen("test_wraparound.log", "w");
    
    // Generate many events to force wraparound
    // TRC_RING_CAP is default 4096
    for (int i = 0; i < 5000; i++) {
        TRC_SCOPE();
        if (i % 100 == 0) {
            TRC_MSG("Event %d", i);
        }
    }
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 8: Message formatting and truncation
TEST(message_formatting) {
    trace::config.out = trace::safe_fopen("test_message_format.log", "w");
    
    TRC_SCOPE();
    
    // Test various format specifiers
    TRC_MSG("Integer: %d", 42);
    TRC_MSG("Float: %.2f", 3.14159);
    TRC_MSG("String: %s", "hello world");
    TRC_MSG("Multiple: %d %s %.1f", 1, "test", 2.5);
    
    // Test very long message (should truncate at TRC_MSG_CAP)
    std::string long_msg(300, 'X');
    TRC_MSG("Long message: %s", long_msg.c_str());
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 9: Timing accuracy
static void timed_function() {
    TRC_SCOPE();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(timing_accuracy) {
    trace::config.out = trace::safe_fopen("test_timing.log", "w");
    trace::config.print_timing = true;
    
    timed_function();
    
    trace::flush_all();
    std::fclose(trace::config.out);
    
    // Verify timing is in reasonable range (40-100ms)
    // Manual verification needed by inspecting log file
    
    trace::config.out = stdout;
}

// Test 10: Binary dump format
TEST(binary_dump) {
    trace::config.out = trace::safe_fopen("test_binary.log", "w");
    
    {
        TRC_SCOPE();
        TRC_MSG("Binary dump test message");
        
        for (int i = 0; i < 5; i++) {
            TRC_SCOPE();
            TRC_MSG("Nested %d", i);
        }
    }
    
    trace::flush_all();
    std::fclose(trace::config.out);
    
    // Test binary dump
    std::string filename = trace::dump_binary("test_comprehensive.bin");
    TEST_ASSERT(!filename.empty(), "Binary dump failed");
    
    // Verify file exists and has content
    FILE* f = trace::safe_fopen("test_comprehensive.bin", "rb");
    TEST_ASSERT(f != nullptr, "Binary file not created");
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    TEST_ASSERT(size > 0, "Binary file is empty");
    
    std::fclose(f);
    trace::config.out = stdout;
}

// Test 11: Auto-flush behavior
TEST(auto_flush) {
    trace::config.out = trace::safe_fopen("test_autoflush.log", "w");
    trace::config.auto_flush_at_exit = true;
    
    {
        TRC_SCOPE();
        TRC_MSG("Auto-flush test");
    }
    
    // Don't manually flush - rely on auto-flush
    
    trace::config.auto_flush_at_exit = false; // Reset
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 12: Thread-local ring independence
TEST(thread_local_independence) {
    trace::config.out = trace::safe_fopen("test_thread_local.log", "w");
    
    std::thread t1([]() {
        TRC_SCOPE();
        TRC_MSG("Thread 1");
    });
    
    std::thread t2([]() {
        TRC_SCOPE();
        TRC_MSG("Thread 2");
    });
    
    {
        TRC_SCOPE();
        TRC_MSG("Main thread");
    }
    
    t1.join();
    t2.join();
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}

