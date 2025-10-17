/**
 * @file example_colors.cpp
 * @brief Example demonstrating ANSI color-coded trace output.
 * 
 * Shows how different call depths are displayed in different colors
 * using a 6-color wheel (Red, Green, Yellow, Blue, Magenta, Cyan).
 * 
 * Best viewed in a terminal that supports ANSI colors.
 */

#include "../include/trace_scope.hpp"
#include <cstdio>

void level3() {
    TRACE_SCOPE();
    TRACE_LOG << "At depth 3 (Blue)";
}

void level2() {
    TRACE_SCOPE();
    TRACE_LOG << "At depth 2 (Yellow)";
    level3();
}

void level1() {
    TRACE_SCOPE();
    TRACE_LOG << "At depth 1 (Green)";
    level2();
}

int main() {
    TRACE_SCOPE();
    
    std::printf("=== ANSI Color-Coded Trace Output ===\n\n");
    std::printf("This example demonstrates depth-based colorization.\n");
    std::printf("Each call depth level gets a different color from a 6-color wheel:\n");
    std::printf("  Depth 1: \033[32mGreen\033[0m\n");
    std::printf("  Depth 2: \033[33mYellow\033[0m\n");
    std::printf("  Depth 3: \033[34mBlue\033[0m\n");
    std::printf("  Depth 4: \033[35mMagenta\033[0m\n");
    std::printf("  Depth 5: \033[36mCyan\033[0m\n");
    std::printf("  Depth 6: \033[31mRed\033[0m (cycles back)\n\n");
    
    // Enable ANSI colors
    trace::config.colorize_depth = true;
    
    std::printf("--- Colorized Output (ANSI) ---\n");
    level1();
    
    trace::flush_all();
    
    std::printf("\n=== Try Different Marker Styles with Colors ===\n\n");
    
    // Unicode style with colors
    trace::config.indent_marker = "│ ";
    trace::config.enter_marker = "↘ ";
    trace::config.exit_marker = "↖ ";
    trace::config.msg_marker = "• ";
    
    std::printf("--- Unicode Markers + Colors ---\n");
    level1();
    trace::flush_all();
    
    std::printf("\n✓ Color demonstration complete\n");
    std::printf("Note: Colors only visible in ANSI-compatible terminals\n");
    
    return 0;
}

