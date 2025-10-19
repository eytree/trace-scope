/**
 * @file example_config_file.cpp
 * @brief Example demonstrating INI configuration file loading.
 * 
 * Shows how to configure trace-scope using an external INI file instead
 * of hardcoding configuration in your source code.
 * 
 * Benefits:
 * - Separate configuration from code
 * - Change settings without recompilation
 * - Easy to share configurations across teams
 * - Version control friendly
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <cstdio>

void worker_function(int id) {
    TRACE_SCOPE();
    TRACE_LOG << "Worker " << id << " starting";
    
    for (int i = 0; i < 3; ++i) {
        TRACE_MSG("Processing item %d", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    TRACE_LOG << "Worker " << id << " complete";
}

int main() {
    std::printf("=== Configuration File Example ===\n\n");
    
    // ========================================================================
    // METHOD 1: Load configuration from file
    // ========================================================================
    std::printf("Method 1: Loading configuration from trace_config.ini...\n");
    
    bool loaded = trace::load_config("../examples/trace_config.ini");
    if (loaded) {
        std::printf("  ✓ Configuration loaded successfully\n");
    } else {
        std::printf("  ⚠ Could not load config file, using defaults\n");
    }
    
    // You can still override specific settings programmatically after loading
    trace::config.print_timestamp = true;  // Override file setting
    
    std::printf("\nConfiguration applied:\n");
    std::printf("  - Output: %s\n", trace::config.out == stdout ? "stdout" : "file");
    std::printf("  - Print timing: %s\n", trace::config.print_timing ? "yes" : "no");
    std::printf("  - Print timestamp: %s\n", trace::config.print_timestamp ? "yes" : "no");
    std::printf("  - Colorize depth: %s\n", trace::config.colorize_depth ? "yes" : "no");
    std::printf("\n");
    
    // Run some traced code
    {
        TRACE_SCOPE();
        TRACE_LOG << "Starting workers";
        
        std::thread t1([](){ worker_function(1); });
        std::thread t2([](){ worker_function(2); });
        
        t1.join();
        t2.join();
        
        TRACE_LOG << "All workers complete";
    }
    
    // Flush all traces
    trace::flush_all();
    
    // ========================================================================
    // METHOD 2: DLL mode with config file
    // ========================================================================
    std::printf("\n=== DLL Mode with Config File ===\n");
    std::printf("In your main.cpp for DLL projects:\n\n");
    std::printf("int main() {\n");
    std::printf("    TRACE_SETUP_DLL_SHARED_WITH_CONFIG(\"trace.conf\");\n");
    std::printf("    // ... rest of code ...\n");
    std::printf("}\n\n");
    std::printf("That's it! One line for DLL setup + config loading!\n");
    
    // Close output file if opened
    if (trace::config.out && trace::config.out != stdout) {
        std::fclose(trace::config.out);
        std::printf("\n✓ Output written to: trace_output.log\n");
    }
    
    std::printf("\n=== Example Complete ===\n");
    return 0;
}

