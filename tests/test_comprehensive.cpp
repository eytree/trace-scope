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
    trace::config.out = std::fopen("test_multithread.log", "w");
    trace::config.mode = trace::TracingMode::Buffered;
    
    auto worker = [](int id) {
        TRACE_SCOPE();
        TRACE_MSG("Worker %d starting", id);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        TRACE_MSG("Worker %d done", id);
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
        trace::config.out = std::fopen("test_immediate.log", "w");
        
        TRACE_SCOPE();
        TRACE_MSG("Immediate mode message 1");
        TRACE_MSG("Immediate mode message 2");
        
        std::fclose(trace::config.out);
        trace::config.mode = trace::TracingMode::Buffered;
    }
    
    // Test buffered mode
    {
        trace::config.out = std::fopen("test_buffered.log", "w");
        
        TRACE_SCOPE();
        TRACE_MSG("Buffered mode message 1");
        TRACE_MSG("Buffered mode message 2");
        
        trace::flush_all();
        std::fclose(trace::config.out);
    }
    
    trace::config.out = stdout;
}

// Test 3: Config option combinations
TEST(config_combinations) {
    trace::config.out = std::fopen("test_config.log", "w");
    
    // Test with all options off
    trace::config.print_timing = false;
    trace::config.print_timestamp = false;
    trace::config.print_thread = false;
    trace::config.include_filename = false;
    trace::config.include_function_name = false;
    
    {
        TRACE_SCOPE();
        TRACE_MSG("Minimal output");
    }
    
    // Test with all options on
    trace::config.print_timing = true;
    trace::config.print_timestamp = true;
    trace::config.print_thread = true;
    trace::config.include_filename = true;
    trace::config.include_function_name = true;
    
    {
        TRACE_SCOPE();
        TRACE_MSG("Full output");
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
    trace::config.out = std::fopen("test_filename_truncation.log", "w");
    trace::config.filename_width = 15;
    
    // Simulate long path by using current file's __FILE__ macro
    TRACE_SCOPE();
    TRACE_MSG("Testing filename truncation with very long path name");
    
    trace::config.filename_width = 20; // Reset
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Helper function with intentionally long name for testing
static void this_is_an_intentionally_very_long_function_name_for_testing_truncation() {
    TRACE_SCOPE();
    TRACE_MSG("Long function name test");
}

// Test 5: Function name truncation
TEST(long_function_truncation) {
    trace::config.out = std::fopen("test_function_truncation.log", "w");
    trace::config.function_width = 15;
    
    this_is_an_intentionally_very_long_function_name_for_testing_truncation();
    
    trace::config.function_width = 20; // Reset
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 6: Deep nesting
static void deeply_nested_call(int depth) {
    TRACE_SCOPE();
    TRACE_MSG("At depth %d", depth);
    if (depth > 0) {
        deeply_nested_call(depth - 1);
    }
}

TEST(deep_nesting) {
    trace::config.out = std::fopen("test_deep_nesting.log", "w");
    
    deeply_nested_call(50);
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 7: Ring buffer wraparound
TEST(ring_buffer_wraparound) {
    trace::config.out = std::fopen("test_wraparound.log", "w");
    
    // Generate many events to force wraparound
    // TRACE_RING_CAP is default 4096
    for (int i = 0; i < 5000; i++) {
        TRACE_SCOPE();
        if (i % 100 == 0) {
            TRACE_MSG("Event %d", i);
        }
    }
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 8: Message formatting and truncation
TEST(message_formatting) {
    trace::config.out = std::fopen("test_message_format.log", "w");
    
    TRACE_SCOPE();
    
    // Test various format specifiers
    TRACE_MSG("Integer: %d", 42);
    TRACE_MSG("Float: %.2f", 3.14159);
    TRACE_MSG("String: %s", "hello world");
    TRACE_MSG("Multiple: %d %s %.1f", 1, "test", 2.5);
    
    // Test very long message (should truncate at TRACE_MSG_CAP)
    std::string long_msg(300, 'X');
    TRACE_MSG("Long message: %s", long_msg.c_str());
    
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 9: Timing accuracy
static void timed_function() {
    TRACE_SCOPE();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(timing_accuracy) {
    trace::config.out = std::fopen("test_timing.log", "w");
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
    trace::config.out = std::fopen("test_binary.log", "w");
    
    {
        TRACE_SCOPE();
        TRACE_MSG("Binary dump test message");
        
        for (int i = 0; i < 5; i++) {
            TRACE_SCOPE();
            TRACE_MSG("Nested %d", i);
        }
    }
    
    trace::flush_all();
    std::fclose(trace::config.out);
    
    // Test binary dump
    bool ok = trace::dump_binary("test_comprehensive.bin");
    TEST_ASSERT(ok, "Binary dump failed");
    
    // Verify file exists and has content
    FILE* f = std::fopen("test_comprehensive.bin", "rb");
    TEST_ASSERT(f != nullptr, "Binary file not created");
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    TEST_ASSERT(size > 0, "Binary file is empty");
    
    std::fclose(f);
    trace::config.out = stdout;
}

// Test 11: Auto-flush behavior
TEST(auto_flush) {
    trace::config.out = std::fopen("test_autoflush.log", "w");
    trace::config.auto_flush_at_exit = true;
    
    {
        TRACE_SCOPE();
        TRACE_MSG("Auto-flush test");
    }
    
    // Don't manually flush - rely on auto-flush
    
    trace::config.auto_flush_at_exit = false; // Reset
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

// Test 12: Thread-local ring independence
TEST(thread_local_independence) {
    trace::config.out = std::fopen("test_thread_local.log", "w");
    
    std::thread t1([]() {
        TRACE_SCOPE();
        TRACE_MSG("Thread 1");
    });
    
    std::thread t2([]() {
        TRACE_SCOPE();
        TRACE_MSG("Thread 2");
    });
    
    {
        TRACE_SCOPE();
        TRACE_MSG("Main thread");
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

