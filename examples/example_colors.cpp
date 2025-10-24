/**
 * @file example_colors.cpp
 * @brief Example demonstrating ANSI color-coded trace output.
 * 
 * Shows how different call depths are displayed in a smooth gradient
 * from green → yellow → orange → red (30 color levels).
 * 
 * Best viewed in a terminal that supports ANSI 256-color mode.
 */

#include <trace-scope/trace_scope.hpp>
#include <cstdio>

void level10() {
    TRC_SCOPE();
    TRC_LOG << "Depth 10 - Yellow-green transition";
}

void level8() {
    TRC_SCOPE();
    TRC_LOG << "Depth 8 - Dark green";
    level10();
}

void level5() {
    TRC_SCOPE();
    TRC_LOG << "Depth 5 - Mid green";
    level8();
}

void level3() {
    TRC_SCOPE();
    TRC_LOG << "Depth 3 - Light-mid green";
    level5();
}

void level2() {
    TRC_SCOPE();
    TRC_LOG << "Depth 2 - Light green";
    level3();
}

void level1() {
    TRC_SCOPE();
    TRC_LOG << "Depth 1 - Lightest green";
    level2();
}

int main() {
    TRC_SCOPE();
    
    std::printf("=== ANSI Color-Coded Trace Output ===\n\n");
    std::printf("This example demonstrates depth-based colorization with a smooth gradient.\n");
    std::printf("The gradient goes from green → yellow → orange → red over 30 levels:\n\n");
    std::printf("  \033[38;5;34mDepth 1-8:   Green shades\033[0m\n");
    std::printf("  \033[38;5;226mDepth 9-12:  Yellow-green\033[0m\n");
    std::printf("  \033[38;5;214mDepth 13-18: Yellow-orange\033[0m\n");
    std::printf("  \033[38;5;196mDepth 19-24: Orange-red\033[0m\n");
    std::printf("  \033[38;5;160mDepth 25-30: Deep red\033[0m\n\n");
    
    // Enable ANSI colors
    trace::config.colorize_depth = true;
    
    std::printf("--- Colorized Output (with gradient) ---\n");
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

