/**
 * @file example_filtering.cpp
 * @brief Demonstrates filtering and selective tracing features.
 * 
 * Shows how to use filters to focus tracing on specific functions,
 * files, or depth ranges using wildcard patterns.
 */

#include <trace-scope/trace_scope.hpp>
#include <cstdio>

// Various test functions to demonstrate filtering
namespace core {
    void important_function() {
        TRC_SCOPE();
        std::printf("  [core] important_function executing\n");
    }
    
    void process_data() {
        TRC_SCOPE();
        std::printf("  [core] process_data executing\n");
        important_function();
    }
}

namespace test {
    void test_basic() {
        TRC_SCOPE();
        std::printf("  [test] test_basic executing\n");
    }
    
    void test_advanced() {
        TRC_SCOPE();
        std::printf("  [test] test_advanced executing\n");
    }
}

namespace debug {
    void debug_helper() {
        TRC_SCOPE();
        std::printf("  [debug] debug_helper executing\n");
    }
    
    void debug_print() {
        TRC_SCOPE();
        std::printf("  [debug] debug_print executing\n");
    }
}

void unfiltered_function() {
    TRC_SCOPE();
    std::printf("  [global] unfiltered_function executing\n");
}

// Deep recursion to demonstrate depth limiting
void recursive_function(int n) {
    TRC_SCOPE();
    if (n > 0) {
        std::printf("  [recursion] depth %d\n", n);
        recursive_function(n - 1);
    }
}

void print_section(const char* title) {
    std::printf("\n");
    std::printf("=============================================================================\n");
    std::printf(" %s\n", title);
    std::printf("=============================================================================\n");
}

void run_all_functions() {
    core::process_data();
    core::important_function();
    test::test_basic();
    test::test_advanced();
    debug::debug_helper();
    debug::debug_print();
    unfiltered_function();
}

int main() {
    std::printf("Filtering and Selective Tracing Example\n");
    std::printf("========================================\n\n");
    
    // Configure basic output
    trace::config.print_timing = true;
    trace::config.print_timestamp = false;
    trace::config.print_thread = false;
    
    // =========================================================================
    // Example 1: No Filters (Baseline)
    // =========================================================================
    print_section("Example 1: No Filters (Trace Everything)");
    run_all_functions();
    trace::flush_all();
    
    // =========================================================================
    // Example 2: Include Only Specific Functions
    // =========================================================================
    print_section("Example 2: Include Only Core Functions (important*, process*)");
    trace::filter_clear();
    trace::filter_include_function("important*");
    trace::filter_include_function("process*");
    run_all_functions();
    trace::flush_all();
    
    // =========================================================================
    // Example 3: Exclude Test Functions
    // =========================================================================
    print_section("Example 3: Exclude Test Functions");
    trace::filter_clear();
    trace::filter_exclude_function("test_*");
    run_all_functions();
    trace::flush_all();
    
    // =========================================================================
    // Example 4: Exclude Multiple Patterns
    // =========================================================================
    print_section("Example 4: Exclude Test and Debug Functions");
    trace::filter_clear();
    trace::filter_exclude_function("test_*");
    trace::filter_exclude_function("debug_*");
    run_all_functions();
    trace::flush_all();
    
    // =========================================================================
    // Example 5: Include Some, But Exclude Specific Functions
    // =========================================================================
    print_section("Example 5: Include *_function, Exclude unfiltered_* (Exclude Wins)");
    trace::filter_clear();
    trace::filter_include_function("*_function");
    trace::filter_exclude_function("unfiltered_*");  // Exclude wins over include
    run_all_functions();
    trace::flush_all();
    
    // =========================================================================
    // Example 6: Depth Limiting
    // =========================================================================
    print_section("Example 6: Max Depth = 3 (Limit Deep Recursion)");
    trace::filter_clear();
    trace::filter_set_max_depth(3);
    std::printf("  Calling recursive_function(10) with max_depth=3:\n");
    recursive_function(10);
    trace::flush_all();
    
    // =========================================================================
    // Example 7: Load Filters from Configuration File
    // =========================================================================
    print_section("Example 7: Load Filters from INI File");
    trace::filter_clear();
    
    // Try to load filter config
    if (trace::load_config("filter_config.ini")) {
        std::printf("  Loaded filters from filter_config.ini\n");
        std::printf("  Run functions with config-based filters:\n");
        run_all_functions();
        trace::flush_all();
    } else {
        std::printf("  Warning: filter_config.ini not found (this is optional)\n");
        std::printf("  Create filter_config.ini with [filter] section to test\n");
    }
    
    // =========================================================================
    // Example 8: File Filtering
    // =========================================================================
    print_section("Example 8: File Filtering");
    trace::filter_clear();
    // Note: __FILE__ gives full path, so we match the basename
    trace::filter_include_file("*example_filtering.cpp");
    run_all_functions();
    trace::flush_all();
    
    // =========================================================================
    // Example 9: Complex Filter Combination
    // =========================================================================
    print_section("Example 9: Complex Combination (Include process*, Max Depth 2)");
    trace::filter_clear();
    trace::filter_include_function("process*");
    trace::filter_include_function("important*");
    trace::filter_set_max_depth(1);  // Only depth 0 and 1
    core::process_data();  // process_data at depth 0, important_function at depth 1
    trace::flush_all();
    
    std::printf("\n");
    std::printf("=============================================================================\n");
    std::printf(" Summary\n");
    std::printf("=============================================================================\n");
    std::printf("Filters allow you to:\n");
    std::printf("  1. Focus on specific namespaces/functions (include patterns)\n");
    std::printf("  2. Exclude noisy functions (exclude patterns)\n");
    std::printf("  3. Limit recursion depth (max_depth)\n");
    std::printf("  4. Filter by file paths\n");
    std::printf("  5. Load filters from INI files\n");
    std::printf("\n");
    std::printf("Wildcard pattern examples:\n");
    std::printf("  core::*        - Match all in core namespace\n");
    std::printf("  *_test         - Match functions ending with _test\n");
    std::printf("  test_*         - Match functions starting with test_\n");
    std::printf("  *debug*        - Match any function with 'debug' in name\n");
    std::printf("\n");
    std::printf("Remember: Exclude always wins over include!\n");
    std::printf("\n");
    
    return 0;
}

