/**
 * @file example_dll_shared.cpp
 * @brief Example demonstrating simple DLL state sharing with TRACE_SETUP_DLL_SHARED().
 * 
 * This example shows how to share trace state across multiple DLLs using
 * the convenience macro. Just one line in main() - automatic setup and cleanup!
 * 
 * Usage scenario:
 * - Large codebase with multiple DLLs
 * - Cannot control which files are compiled
 * - Need unified trace output from all DLLs
 * 
 * New simple approach: Just use TRACE_SETUP_DLL_SHARED() macro!
 */

#include <trace-scope/trace_scope.hpp>
#include <cstdio>

// ============================================================================
// OLD APPROACH (for reference - still works if you need manual control)
// ============================================================================
// namespace {
//     trace::Config g_trace_config;
//     trace::Registry g_trace_registry;
//     
//     struct TraceInit {
//         TraceInit() {
//             trace::set_external_state(&g_trace_config, &g_trace_registry);
//             g_trace_config.out = std::fopen("dll_shared.log", "w");
//         }
//         ~TraceInit() {
//             trace::flush_all();
//             if (g_trace_config.out && g_trace_config.out != stdout) {
//                 std::fclose(g_trace_config.out);
//             }
//         }
//     } g_trace_init;
// }
// ============================================================================

// ============================================================================
// Use tracing normally - works across all DLLs!
// ============================================================================

/**
 * @brief Simulates a function in DLL #1
 */
void dll1_function() {
    TRACE_SCOPE();
    TRACE_LOG << "This would be in DLL #1";  // Stream-based logging
}

/**
 * @brief Simulates a function in DLL #2
 */
void dll2_function() {
    TRACE_SCOPE();
    TRACE_LOG << "This would be in DLL #2";  // Stream-based logging
    dll1_function();  // Cross-DLL call
}

/**
 * @brief Main function (in main executable)
 */
int main() {
    // ========================================================================
    // SIMPLE SETUP: Just one line for DLL state sharing!
    // ========================================================================
    TRACE_SETUP_DLL_SHARED();  // Automatic setup & cleanup via RAII
    
    // Configure trace output (use get_config() to access the shared config)
    trace::get_config().out = std::fopen("dll_shared.log", "w");
    trace::get_config().print_timestamp = false;
    trace::get_config().print_thread = true;
    
    TRACE_SCOPE();
    
    std::printf("=== DLL State Sharing Example (Simplified) ===\n");
    std::printf("Setup: TRACE_SETUP_DLL_SHARED() - just 1 line!\n");
    std::printf("All traces will be written to dll_shared.log\n\n");
    
    // Call functions from different "DLLs"
    dll1_function();
    dll2_function();
    
    std::printf("\nTraces written to dll_shared.log\n");
    std::printf("All DLLs shared the same trace state!\n");
    
    // Manual flush before file closure
    // Note: The RAII guard also flushes, but we do it here to ensure
    // the file is written before we close it
    trace::flush_all();
    
    if (trace::get_config().out && trace::get_config().out != stdout) {
        std::fclose(trace::get_config().out);
        trace::get_config().out = stdout;  // Reset to avoid double-close
    }
    
    std::printf("\n✓ Automatic cleanup will happen on exit via RAII guard\n");
    
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

