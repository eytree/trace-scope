/**
 * @file example_custom_markers.cpp
 * @brief Example demonstrating customizable visual markers.
 * 
 * Shows how to customize indent, entry, exit, and message markers
 * with different styles (ASCII, Unicode, box-drawing).
 */

#include <trace-scope/trace_scope.hpp>
#include <cstdio>

void inner_function() {
    TRC_SCOPE();
    TRC_LOG << "Inside inner function";
}

void outer_function() {
    TRC_SCOPE();
    TRC_MSG("Calling inner from outer");
    inner_function();
}

int main() {
    TRC_SCOPE();
    
    std::printf("=== Testing Different Marker Styles ===\n\n");
    
    // Style 1: Default ASCII markers
    std::printf("--- Default ASCII Style ---\n");
    trace::config.indent_marker = "| ";
    trace::config.enter_marker = "-> ";
    trace::config.exit_marker = "<- ";
    trace::config.msg_marker = "- ";
    
    outer_function();
    trace::flush_all();
    
    // Style 2: Unicode arrows
    std::printf("\n--- Unicode Arrow Style ---\n");
    trace::config.indent_marker = "│ ";
    trace::config.enter_marker = "↘ ";
    trace::config.exit_marker = "↖ ";
    trace::config.msg_marker = "• ";
    
    outer_function();
    trace::flush_all();
    
    // Style 3: Box-drawing characters
    std::printf("\n--- Box-Drawing Style ---\n");
    trace::config.indent_marker = "├─";
    trace::config.enter_marker = "┌ ";
    trace::config.exit_marker = "└ ";
    trace::config.msg_marker = "│ ";
    
    outer_function();
    trace::flush_all();
    
    // Style 4: Minimal ASCII
    std::printf("\n--- Minimal ASCII Style ---\n");
    trace::config.indent_marker = "  ";
    trace::config.enter_marker = "> ";
    trace::config.exit_marker = "< ";
    trace::config.msg_marker = ". ";
    
    outer_function();
    trace::flush_all();
    
    // Style 5: No indent markers
    std::printf("\n--- No Indent Markers (Plain Whitespace) ---\n");
    trace::config.show_indent_markers = false;
    trace::config.enter_marker = "ENTER ";
    trace::config.exit_marker = "EXIT  ";
    trace::config.msg_marker = "LOG   ";
    
    outer_function();
    trace::flush_all();
    
    std::printf("\n=== All marker styles demonstrated ===\n");
    return 0;
}

