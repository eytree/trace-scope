/**
 * @file example_dll_shared.cpp
 * @brief Example demonstrating header-only DLL state sharing.
 * 
 * This example shows how to share trace state across multiple DLLs
 * without requiring any .cpp compilation. Purely header-only approach.
 * 
 * Usage scenario:
 * - Large codebase with multiple DLLs
 * - Cannot control which files are compiled
 * - Need unified trace output from all DLLs
 */

#include "../include/trace_scope.hpp"
#include <cstdio>

// ============================================================================
// STEP 1: Create shared state instances (in main executable or common header)
// ============================================================================

namespace {
    // These instances will be shared across all DLLs in the process
    trace::Config g_trace_config;
    trace::Registry g_trace_registry;
    
    // Optional: Initialize automatically via static constructor
    struct TraceInit {
        TraceInit() {
            // Set external state BEFORE any tracing occurs
            trace::set_external_state(&g_trace_config, &g_trace_registry);
            
            // Configure trace output
            g_trace_config.out = std::fopen("dll_shared.log", "w");
            g_trace_config.print_timestamp = false;
            g_trace_config.print_thread = true;
        }
        
        ~TraceInit() {
            // Flush and cleanup
            trace::flush_all();
            if (g_trace_config.out && g_trace_config.out != stdout) {
                std::fclose(g_trace_config.out);
            }
        }
    } g_trace_init;  // Constructed before main(), destructed after main()
}

// ============================================================================
// STEP 2: Use tracing normally - works across all DLLs!
// ============================================================================

/**
 * @brief Simulates a function in DLL #1
 */
void dll1_function() {
    TRACE_SCOPE();
    TRACE_MSG("This would be in DLL #1");
}

/**
 * @brief Simulates a function in DLL #2
 */
void dll2_function() {
    TRACE_SCOPE();
    TRACE_MSG("This would be in DLL #2");
    dll1_function();  // Cross-DLL call
}

/**
 * @brief Main function (in main executable)
 */
int main() {
    TRACE_SCOPE();
    
    std::printf("=== DLL State Sharing Example ===\n");
    std::printf("All traces will be written to dll_shared.log\n\n");
    
    // Call functions from different "DLLs"
    dll1_function();
    dll2_function();
    
    std::printf("\nTraces written to dll_shared.log\n");
    std::printf("All DLLs shared the same trace state!\n");
    
    // Note: g_trace_init destructor will automatically flush and cleanup
    return 0;
}

/**
 * ## Expected Output in dll_shared.log:
 * 
 * (thread_id) example_dll_shared.cpp:  75 main                 -> main
 * (thread_id) example_dll_shared.cpp:  83 main                   - All traces will be written to dll_shared.log
 * (thread_id) example_dll_shared.cpp:  56 dll1_function           -> dll1_function
 * (thread_id) example_dll_shared.cpp:  57 dll1_function             - This would be in DLL #1
 * (thread_id) example_dll_shared.cpp:  56 dll1_function           <- dll1_function  [X.XX us]
 * (thread_id) example_dll_shared.cpp:  64 dll2_function           -> dll2_function
 * (thread_id) example_dll_shared.cpp:  65 dll2_function             - This would be in DLL #2
 * (thread_id) example_dll_shared.cpp:  56 dll1_function             -> dll1_function
 * (thread_id) example_dll_shared.cpp:  57 dll1_function               - This would be in DLL #1
 * (thread_id) example_dll_shared.cpp:  56 dll1_function             <- dll1_function  [X.XX us]
 * (thread_id) example_dll_shared.cpp:  64 dll2_function           <- dll2_function  [X.XX us]
 * (thread_id) example_dll_shared.cpp:  75 main                 <- main  [X.XX ms]
 * 
 * ## Key Benefits:
 * - Completely header-only (no .cpp compilation needed)
 * - Works across any number of DLLs
 * - Simple 1-line setup: trace::set_external_state()
 * - Backward compatible (optional feature)
 * - Zero overhead when not used
 */

