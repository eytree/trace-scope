/**
 * @file test_python_tool.cpp
 * @brief Simple program to generate binary trace for testing Python tool.
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>

void test_function() {
    TRC_SCOPE();
    TRC_MSG("Test function called");
}

void core_process() {
    TRC_SCOPE();
    test_function();
}

void debug_helper() {
    TRC_SCOPE();
    TRC_MSG("Debug helper");
}

void worker_thread(int id) {
    TRC_SCOPE();
    TRC_MSG("Worker %d", id);
    core_process();
}

int main() {
    // Generate some traces
    TRC_SCOPE();
    
    core_process();
    test_function();
    debug_helper();
    
    // Multi-threaded
    std::thread t1([](){ worker_thread(1); });
    std::thread t2([](){ worker_thread(2); });
    
    t1.join();
    t2.join();
    
    // Dump to binary
    std::string filename = trace::dump_binary("test_trace");
    if (!filename.empty()) {
        std::printf("✓ Generated %s\n", filename.c_str());
    } else {
        std::printf("✗ Failed to generate binary\n");
        return 1;
    }
    
    return 0;
}

