/**
 * @file example_test_v08.cpp
 * @brief Test example for v0.8.0-alpha features
 * 
 * Tests:
 * - .trc file extension
 * - Output directory creation
 * - Different layout modes (Flat, ByDate, BySession)
 * - Python analyzer directory processing
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>

void worker_function(int id) {
    TRACE_SCOPE();
    TRACE_MSG("Worker %d started", id);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    TRACE_MSG("Worker %d finished", id);
}

void test_function() {
    TRACE_SCOPE();
    for (int i = 0; i < 3; ++i) {
        TRACE_MSG("Iteration %d", i);
        worker_function(i);
    }
}

int main() {
    std::printf("trace-scope v%s Test Example\n", TRACE_SCOPE_VERSION);
    std::printf("==========================================\n\n");
    
    // Test 1: Flat layout in "test_output" directory
    std::printf("Test 1: Flat layout\n");
    trace::config.output_dir = "test_output";
    trace::config.output_layout = trace::Config::OutputLayout::Flat;
    trace::config.dump_prefix = "test";
    
    {
        TRACE_SCOPE();
        test_function();
    }
    trace::flush_all();
    
    std::string file1 = trace::dump_binary();
    std::printf("  Generated: %s\n\n", file1.c_str());
    
    // Small delay to ensure different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test 2: ByDate layout
    std::printf("Test 2: ByDate layout\n");
    trace::config.output_layout = trace::Config::OutputLayout::ByDate;
    
    {
        TRACE_SCOPE();
        test_function();
    }
    trace::flush_all();
    
    std::string file2 = trace::dump_binary();
    std::printf("  Generated: %s\n\n", file2.c_str());
    
    // Small delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test 3: BySession layout with auto-increment
    std::printf("Test 3: BySession layout (auto-increment)\n");
    trace::config.output_layout = trace::Config::OutputLayout::BySession;
    trace::config.current_session = 0;  // auto-increment
    
    {
        TRACE_SCOPE();
        test_function();
    }
    trace::flush_all();
    
    std::string file3 = trace::dump_binary();
    std::printf("  Generated: %s\n\n", file3.c_str());
    
    // Test 4: Another session dump (should increment to session_002)
    {
        TRACE_SCOPE();
        worker_function(999);
    }
    trace::flush_all();
    
    std::string file4 = trace::dump_binary();
    std::printf("  Generated: %s\n\n", file4.c_str());
    
    std::printf("==========================================\n");
    std::printf("All tests completed!\n\n");
    std::printf("To test Python analyzer:\n");
    std::printf("  python tools/trc_analyze.py --version\n");
    std::printf("  python tools/trc_analyze.py display test_output/\n");
    std::printf("  python tools/trc_analyze.py display test_output/ --recursive\n");
    std::printf("  python tools/trc_analyze.py stats test_output/ --recursive\n");
    
    return 0;
}

