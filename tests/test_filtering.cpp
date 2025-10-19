/**
 * @file test_filtering.cpp
 * @brief Comprehensive tests for filtering and selective tracing.
 */

#include <trace-scope/trace_scope.hpp>
#include "test_framework.hpp"
#include <cstdio>
#include <cstring>

// Simple assertion macros without message for readability - redefine to use single arg
#undef TEST_ASSERT
#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            char buf[512]; \
            std::snprintf(buf, sizeof(buf), "%s:%d: Assertion failed: %s", \
                         __FILE__, __LINE__, #cond); \
            throw test_framework::AssertionFailure(buf); \
        } \
    } while (0)

#undef TEST_ASSERT_EQ
#define TEST_ASSERT_EQ(a, b) \
    do { \
        auto val_a = (a); \
        auto val_b = (b); \
        if (!(val_a == val_b)) { \
            char buf[512]; \
            std::snprintf(buf, sizeof(buf), "%s:%d: Assertion failed: %s == %s", \
                         __FILE__, __LINE__, #a, #b); \
            throw test_framework::AssertionFailure(buf); \
        } \
    } while (0)

// Test helper functions with various names
void test_function() { TRACE_SCOPE(); }
void test_another() { TRACE_SCOPE(); }
void my_function() { TRACE_SCOPE(); }
void production_code() { TRACE_SCOPE(); }
void debug_helper() { TRACE_SCOPE(); }
void core_process() { TRACE_SCOPE(); }

// Recursive function for depth testing
void recursive(int depth) {
    TRACE_SCOPE();
    if (depth > 0) recursive(depth - 1);
}

// Helper to count trace events
int count_events_in_buffer() {
    // Flush to a temp file and count lines
    FILE* tmp = std::tmpfile();
    if (!tmp) return -1;
    
    FILE* old_out = trace::config.out;
    trace::config.out = tmp;
    trace::flush_all();
    trace::config.out = old_out;
    
    std::rewind(tmp);
    int count = 0;
    char line[256];
    while (std::fgets(line, sizeof(line), tmp)) {
        if (std::strlen(line) > 1) ++count;  // Count non-empty lines
    }
    
    std::fclose(tmp);
    return count;
}

TEST(wildcard_match_exact) {
    TEST_ASSERT(trace::filter_utils::wildcard_match("test", "test"));
    TEST_ASSERT(!trace::filter_utils::wildcard_match("test", "testing"));
    TEST_ASSERT(!trace::filter_utils::wildcard_match("test", "tes"));
}

TEST(wildcard_match_star_suffix) {
    TEST_ASSERT(trace::filter_utils::wildcard_match("test_*", "test_"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("test_*", "test_foo"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("test_*", "test_bar_baz"));
    TEST_ASSERT(!trace::filter_utils::wildcard_match("test_*", "testing"));
    TEST_ASSERT(!trace::filter_utils::wildcard_match("test_*", "my_test"));
}

TEST(wildcard_match_star_prefix) {
    TEST_ASSERT(trace::filter_utils::wildcard_match("*_test", "my_test"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*_test", "foo_bar_test"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*_test", "_test"));
    TEST_ASSERT(!trace::filter_utils::wildcard_match("*_test", "test"));
    TEST_ASSERT(!trace::filter_utils::wildcard_match("*_test", "testing"));
}

TEST(wildcard_match_star_middle) {
    TEST_ASSERT(trace::filter_utils::wildcard_match("*mid*", "mid"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*mid*", "middle"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*mid*", "pyramid"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*mid*", "amid"));
    TEST_ASSERT(!trace::filter_utils::wildcard_match("*mid*", "test"));
}

TEST(wildcard_match_star_only) {
    TEST_ASSERT(trace::filter_utils::wildcard_match("*", "anything"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*", "test_foo"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*", ""));
}

TEST(wildcard_match_multiple_stars) {
    TEST_ASSERT(trace::filter_utils::wildcard_match("*::*", "namespace::function"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("test_*_*", "test_foo_bar"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*a*b*", "ab"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*a*b*", "aXb"));
    TEST_ASSERT(trace::filter_utils::wildcard_match("*a*b*", "XaYbZ"));
}

TEST(wildcard_match_null_checks) {
    TEST_ASSERT(!trace::filter_utils::wildcard_match(nullptr, "test"));
    TEST_ASSERT(!trace::filter_utils::wildcard_match("test", nullptr));
    TEST_ASSERT(!trace::filter_utils::wildcard_match(nullptr, nullptr));
}

TEST(function_include_single_pattern) {
    trace::filter_clear();
    trace::filter_include_function("test_*");
    
    // Directly test should_trace since we can't easily intercept Ring::write
    TEST_ASSERT(trace::filter_utils::should_trace("test_function", "file.cpp", 0));
    TEST_ASSERT(trace::filter_utils::should_trace("test_another", "file.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("my_function", "file.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("production_code", "file.cpp", 0));
}

TEST(function_include_multiple_patterns) {
    trace::filter_clear();
    trace::filter_include_function("test_*");
    trace::filter_include_function("core_*");
    
    TEST_ASSERT(trace::filter_utils::should_trace("test_function", "file.cpp", 0));
    TEST_ASSERT(trace::filter_utils::should_trace("core_process", "file.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("my_function", "file.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("debug_helper", "file.cpp", 0));
}

TEST(function_exclude_single_pattern) {
    trace::filter_clear();
    trace::filter_exclude_function("debug_*");
    
    TEST_ASSERT(trace::filter_utils::should_trace("test_function", "file.cpp", 0));
    TEST_ASSERT(trace::filter_utils::should_trace("my_function", "file.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("debug_helper", "file.cpp", 0));
}

TEST(function_exclude_multiple_patterns) {
    trace::filter_clear();
    trace::filter_exclude_function("test_*");
    trace::filter_exclude_function("debug_*");
    
    TEST_ASSERT(trace::filter_utils::should_trace("my_function", "file.cpp", 0));
    TEST_ASSERT(trace::filter_utils::should_trace("production_code", "file.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("test_function", "file.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("debug_helper", "file.cpp", 0));
}

TEST(function_exclude_wins_over_include) {
    trace::filter_clear();
    trace::filter_include_function("test_*");
    trace::filter_exclude_function("test_function");  // Exclude specific
    
    TEST_ASSERT(!trace::filter_utils::should_trace("test_function", "file.cpp", 0));  // Excluded
    TEST_ASSERT(trace::filter_utils::should_trace("test_another", "file.cpp", 0));   // Included
    TEST_ASSERT(!trace::filter_utils::should_trace("my_function", "file.cpp", 0));   // Not in include list
}

TEST(file_include_pattern) {
    trace::filter_clear();
    trace::filter_include_file("src/core/*");
    
    TEST_ASSERT(trace::filter_utils::should_trace("func", "src/core/main.cpp", 0));
    TEST_ASSERT(trace::filter_utils::should_trace("func", "src/core/sub/file.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("func", "src/test/main.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("func", "other.cpp", 0));
}

TEST(file_exclude_pattern) {
    trace::filter_clear();
    trace::filter_exclude_file("*/test/*");
    
    TEST_ASSERT(trace::filter_utils::should_trace("func", "src/core/main.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("func", "src/test/main.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("func", "lib/test/file.cpp", 0));
}

TEST(file_exclude_wins_over_include) {
    trace::filter_clear();
    trace::filter_include_file("src/*");
    trace::filter_exclude_file("*/test/*");
    
    TEST_ASSERT(trace::filter_utils::should_trace("func", "src/core/main.cpp", 0));
    TEST_ASSERT(!trace::filter_utils::should_trace("func", "src/test/main.cpp", 0));  // Excluded
}

TEST(max_depth_unlimited) {
    trace::filter_clear();
    trace::filter_set_max_depth(-1);  // Unlimited
    
    TEST_ASSERT(trace::filter_utils::should_trace("func", "file.cpp", 0));
    TEST_ASSERT(trace::filter_utils::should_trace("func", "file.cpp", 10));
    TEST_ASSERT(trace::filter_utils::should_trace("func", "file.cpp", 100));
}

TEST(max_depth_limited) {
    trace::filter_clear();
    trace::filter_set_max_depth(5);
    
    TEST_ASSERT(trace::filter_utils::should_trace("func", "file.cpp", 0));
    TEST_ASSERT(trace::filter_utils::should_trace("func", "file.cpp", 5));
    TEST_ASSERT(!trace::filter_utils::should_trace("func", "file.cpp", 6));
    TEST_ASSERT(!trace::filter_utils::should_trace("func", "file.cpp", 10));
}

TEST(empty_filters_trace_all) {
    trace::filter_clear();
    
    TEST_ASSERT(trace::filter_utils::should_trace("any_function", "any_file.cpp", 0));
    TEST_ASSERT(trace::filter_utils::should_trace("test_function", "test.cpp", 10));
    TEST_ASSERT(trace::filter_utils::should_trace("debug_helper", "debug.cpp", 100));
}

TEST(filter_clear_resets_all) {
    // Set up various filters
    trace::filter_include_function("test_*");
    trace::filter_exclude_function("debug_*");
    trace::filter_include_file("src/*");
    trace::filter_exclude_file("*/test/*");
    trace::filter_set_max_depth(5);
    
    // Clear all
    trace::filter_clear();
    
    // Verify everything is reset
    const auto& f = trace::get_config().filter;
    TEST_ASSERT(f.include_functions.empty());
    TEST_ASSERT(f.exclude_functions.empty());
    TEST_ASSERT(f.include_files.empty());
    TEST_ASSERT(f.exclude_files.empty());
    TEST_ASSERT_EQ(f.max_depth, -1);
}

TEST(filter_with_null_function) {
    trace::filter_clear();
    trace::filter_include_function("test_*");
    
    // Null function pointer (e.g., for TRACE_MSG)
    // Should pass if file filter allows it
    TEST_ASSERT(trace::filter_utils::should_trace(nullptr, "file.cpp", 0));
}

TEST(filter_complex_combination) {
    trace::filter_clear();
    trace::filter_include_function("*core*");
    trace::filter_exclude_function("*debug*");
    trace::filter_include_file("src/*");
    trace::filter_exclude_file("*/test/*");
    trace::filter_set_max_depth(10);
    
    // Should trace: matches function include, not excluded, file matches, depth ok
    TEST_ASSERT(trace::filter_utils::should_trace("core_process", "src/main.cpp", 5));
    
    // Should NOT trace: function excluded
    TEST_ASSERT(!trace::filter_utils::should_trace("core_debug", "src/main.cpp", 5));
    
    // Should NOT trace: file excluded
    TEST_ASSERT(!trace::filter_utils::should_trace("core_process", "src/test/main.cpp", 5));
    
    // Should NOT trace: depth exceeded
    TEST_ASSERT(!trace::filter_utils::should_trace("core_process", "src/main.cpp", 11));
    
    // Should NOT trace: function not in include list
    TEST_ASSERT(!trace::filter_utils::should_trace("other_function", "src/main.cpp", 5));
}

TEST(filter_from_ini_file) {
    // Create a temporary INI file with filter config
    FILE* f = std::fopen("test_filter_temp.ini", "w");
    TEST_ASSERT(f != nullptr);
    
    std::fprintf(f, "[filter]\n");
    std::fprintf(f, "include_function = core_*\n");
    std::fprintf(f, "exclude_function = *_debug\n");
    std::fprintf(f, "include_file = src/*\n");
    std::fprintf(f, "max_depth = 8\n");
    std::fclose(f);
    
    // Load config
    trace::filter_clear();
    bool loaded = trace::load_config("test_filter_temp.ini");
    TEST_ASSERT(loaded);
    
    // Verify filters were loaded
    const auto& filter = trace::get_config().filter;
    TEST_ASSERT(!filter.include_functions.empty());
    TEST_ASSERT(!filter.exclude_functions.empty());
    TEST_ASSERT(!filter.include_files.empty());
    TEST_ASSERT_EQ(filter.max_depth, 8);
    
    // Test the loaded filters
    TEST_ASSERT(trace::filter_utils::should_trace("core_process", "src/main.cpp", 5));
    TEST_ASSERT(!trace::filter_utils::should_trace("core_debug", "src/main.cpp", 5));
    TEST_ASSERT(!trace::filter_utils::should_trace("core_process", "lib/main.cpp", 5));
    TEST_ASSERT(!trace::filter_utils::should_trace("core_process", "src/main.cpp", 9));
    
    // Cleanup
    std::remove("test_filter_temp.ini");
}

int main(int argc, char** argv) {
    // Ensure we start with clean config
    trace::filter_clear();
    trace::config.out = nullptr;  // Don't actually output during tests
    
    return run_tests(argc, argv);
}

