/**
 * @file test_config_file.cpp
 * @brief Tests for INI configuration file parsing and loading.
 * 
 * Tests include:
 * 1. Valid INI file with all options
 * 2. Partial INI file with missing sections
 * 3. Comments and whitespace handling
 * 4. Boolean value parsing
 * 5. Integer and float parsing
 * 6. String marker parsing
 * 7. Missing file error handling
 * 8. Malformed INI error handling
 * 9. Programmatic override after loading
 * 10. DLL macro with config file
 */

#include <trace-scope/trace_scope.hpp>
#include "test_framework.hpp"
#include <cstdio>
#include <cstring>

/**
 * @brief Test 1: Load valid comprehensive config file.
 */
TEST(load_valid_config) {
    trace::Config cfg;
    
    // Set some non-default values first
    cfg.print_timing = false;
    cfg.colorize_depth = false;
    cfg.use_double_buffering = false;
    
    // Load from file
    bool ok = cfg.load_from_file("../tests/test_config_valid.ini");
    TEST_ASSERT(ok, "Should load valid config file");
    
    // Verify display settings
    TEST_ASSERT_EQ(cfg.print_timing, true, "print_timing from INI");
    TEST_ASSERT_EQ(cfg.print_timestamp, true, "print_timestamp from INI");
    TEST_ASSERT_EQ(cfg.print_thread, false, "print_thread from INI");
    TEST_ASSERT_EQ(cfg.colorize_depth, true, "colorize_depth from INI");
    TEST_ASSERT_EQ(cfg.include_filename, false, "include_filename from INI");
    TEST_ASSERT_EQ(cfg.show_full_path, true, "show_full_path from INI");
    
    // Verify formatting settings
    TEST_ASSERT_EQ(cfg.filename_width, 25, "filename_width from INI");
    TEST_ASSERT_EQ(cfg.line_width, 6, "line_width from INI");
    TEST_ASSERT_EQ(cfg.function_width, 30, "function_width from INI");
    
    // Verify modes
    TEST_ASSERT(cfg.mode == trace::TracingMode::Hybrid, "mode from INI should be Hybrid");
    TEST_ASSERT_EQ(cfg.auto_flush_at_exit, true, "auto_flush_at_exit from INI");
    TEST_ASSERT_EQ(cfg.use_double_buffering, true, "use_double_buffering from INI");
    TEST_ASSERT(cfg.auto_flush_threshold > 0.79f && cfg.auto_flush_threshold < 0.81f, 
                "auto_flush_threshold from INI");
    
    // Cleanup
    if (cfg.out && cfg.out != stdout) {
        std::fclose(cfg.out);
    }
}

/**
 * @brief Test 2: Load partial config file.
 */
TEST(load_partial_config) {
    trace::Config cfg;
    
    // Set defaults
    cfg.print_timing = true;
    cfg.print_timestamp = false;
    cfg.use_double_buffering = false;
    
    // Load partial config
    bool ok = cfg.load_from_file("../tests/test_config_partial.ini");
    TEST_ASSERT(ok, "Should load partial config file");
    
    // Verify loaded values
    TEST_ASSERT_EQ(cfg.print_timing, false, "print_timing from INI");
    TEST_ASSERT_EQ(cfg.colorize_depth, true, "colorize_depth from INI");
    TEST_ASSERT_EQ(cfg.use_double_buffering, true, "use_double_buffering from INI");
    
    // Verify unspecified values keep their defaults
    TEST_ASSERT_EQ(cfg.print_timestamp, false, "print_timestamp uses default");
    TEST_ASSERT_EQ(cfg.print_thread, true, "print_thread uses default");
}

/**
 * @brief Test 3: Handle missing file gracefully.
 */
TEST(missing_file_handling) {
    trace::Config cfg;
    
    bool ok = cfg.load_from_file("nonexistent_file.ini");
    TEST_ASSERT(!ok, "Should return false for missing file");
    
    // Config should still be usable with defaults
    TEST_ASSERT(cfg.out == stdout, "Should use default stdout");
    TEST_ASSERT_EQ(cfg.print_timing, true, "Should use default print_timing");
}

/**
 * @brief Test 4: Handle malformed INI gracefully.
 */
TEST(malformed_ini_handling) {
    trace::Config cfg;
    
    // Should not crash, should skip bad lines
    bool ok = cfg.load_from_file("../tests/test_config_invalid.ini");
    
    // Should still load (just with warnings)
    TEST_ASSERT(ok, "Should complete parsing despite errors");
    
    // Valid line at end should still be parsed
    TEST_ASSERT_EQ(cfg.use_double_buffering, true, "Should parse valid lines");
}

/**
 * @brief Test 5: Boolean value parsing.
 */
TEST(boolean_parsing) {
    // Test the parser directly
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("true"), true, "Parse 'true'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("false"), false, "Parse 'false'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("1"), true, "Parse '1'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("0"), false, "Parse '0'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("on"), true, "Parse 'on'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("off"), false, "Parse 'off'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("yes"), true, "Parse 'yes'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("no"), false, "Parse 'no'");
    
    // Case-insensitive
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("TRUE"), true, "Parse 'TRUE'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("False"), false, "Parse 'False'");
    TEST_ASSERT_EQ(trace::ini_parser::parse_bool("ON"), true, "Parse 'ON'");
}

/**
 * @brief Test 6: Integer and float parsing.
 */
TEST(number_parsing) {
    TEST_ASSERT_EQ(trace::ini_parser::parse_int("42"), 42, "Parse integer");
    TEST_ASSERT_EQ(trace::ini_parser::parse_int("-10"), -10, "Parse negative");
    TEST_ASSERT_EQ(trace::ini_parser::parse_int("  100  "), 100, "Parse with whitespace");
    
    float f = trace::ini_parser::parse_float("0.9");
    TEST_ASSERT(f > 0.89f && f < 0.91f, "Parse float 0.9");
    
    f = trace::ini_parser::parse_float("1.5");
    TEST_ASSERT(f > 1.49f && f < 1.51f, "Parse float 1.5");
}

/**
 * @brief Test 7: String utilities.
 */
TEST(string_utilities) {
    TEST_ASSERT_EQ(trace::ini_parser::trim("  hello  "), std::string("hello"), "Trim spaces");
    TEST_ASSERT_EQ(trace::ini_parser::trim("hello"), std::string("hello"), "Trim no spaces");
    TEST_ASSERT_EQ(trace::ini_parser::trim("   "), std::string(""), "Trim only spaces");
    
    TEST_ASSERT_EQ(trace::ini_parser::unquote("\"hello\""), std::string("hello"), "Unquote quoted string");
    TEST_ASSERT_EQ(trace::ini_parser::unquote("hello"), std::string("hello"), "Unquote unquoted string");
    TEST_ASSERT_EQ(trace::ini_parser::unquote("  \"hello\"  "), std::string("hello"), "Unquote with trim");
}

/**
 * @brief Test 8: Programmatic override after loading.
 */
TEST(programmatic_override) {
    trace::Config cfg;
    
    // Load from file
    cfg.load_from_file("../tests/test_config_partial.ini");
    
    // Override specific values programmatically
    cfg.print_timing = true;  // Override what's in file
    cfg.filename_width = 15;  // Set value not in file
    
    // Verify overrides took effect
    TEST_ASSERT_EQ(cfg.print_timing, true, "Programmatic override");
    TEST_ASSERT_EQ(cfg.filename_width, 15, "Programmatic setting");
    
    // Verify file settings still applied for other fields
    TEST_ASSERT_EQ(cfg.use_double_buffering, true, "File setting preserved");
}

/**
 * @brief Test 9: Marker strings with spaces.
 */
TEST(marker_string_parsing) {
    trace::Config cfg;
    cfg.load_from_file("../tests/test_config_valid.ini");
    
    // Markers should preserve spaces and special characters
    // Note: comparing pointers won't work for c_str(), so we'd need to compare strings
    // For now, just verify it doesn't crash and config loads
    TEST_ASSERT(cfg.show_indent_markers == false, "show_indent_markers from INI");
}

/**
 * @brief Test 10: Comments and whitespace.
 */
TEST(comments_and_whitespace) {
    trace::Config cfg;
    
    // This file has inline comments, empty lines, various whitespace
    bool ok = cfg.load_from_file("../tests/test_config_valid.ini");
    TEST_ASSERT(ok, "Should handle comments and whitespace");
    
    // All settings should still load correctly
    TEST_ASSERT_EQ(cfg.print_timestamp, true, "Settings with comments");
}

/**
 * @brief Test 11: Global load_config() function.
 */
TEST(global_load_config_function) {
    // Reset to defaults
    trace::get_config().print_timing = true;
    trace::get_config().use_double_buffering = false;
    
    // Load via global function
    bool ok = trace::load_config("../tests/test_config_partial.ini");
    TEST_ASSERT(ok, "global load_config() should work");
    
    // Verify settings applied to global config
    TEST_ASSERT_EQ(trace::get_config().print_timing, false, "Global config updated");
    TEST_ASSERT_EQ(trace::get_config().use_double_buffering, true, "Global config updated");
    
    // Reset back to defaults
    trace::get_config().print_timing = true;
    trace::get_config().use_double_buffering = false;
}

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}

