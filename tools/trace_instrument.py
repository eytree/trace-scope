#!/usr/bin/env python3
"""
Automatic instrumentation tool for trace_scope.

Usage:
    python trace_instrument.py add <file.cpp>      # Add TRACE_SCOPE() to all functions
    python trace_instrument.py remove <file.cpp>   # Remove all TRACE_SCOPE() calls
    python trace_instrument.py --help              # Show help
    
This tool uses simple regex-based parsing to find function definitions
and automatically insert or remove TRACE_SCOPE() macros.

Notes:
- Creates backup files (.bak) before modifying
- Handles most common C++ function patterns
- May need manual adjustment for complex templates or macros
"""

import re
import sys
import os
import argparse
from typing import List, Tuple

def find_function_bodies(content: str) -> List[Tuple[int, int, str]]:
    """
    Find function body positions in the source code.
    
    Returns list of (start_pos, open_brace_pos, function_signature) tuples.
    Simple heuristic: looks for patterns like:
        return_type function_name(...) {
    or  return_type ClassName::method_name(...) {
    """
    functions = []
    
    # Pattern to match function definitions
    # Matches: [return_type] name(...) { or name(...) const {
    # This is a simplified pattern - doesn't handle all C++ edge cases
    pattern = r'''
        (?:^|\n)                    # Start of line
        \s*                         # Optional whitespace
        (?:[\w:*&<>,\s]+\s+)?       # Optional return type (can be complex)
        ([\w~]+(?:::\w+)?)          # Function name (possibly qualified)
        \s*\(                       # Opening parenthesis
        [^)]*                       # Parameters
        \)                          # Closing parenthesis
        \s*                         # Whitespace
        (?:const\s*)?               # Optional const
        (?:override\s*)?            # Optional override
        (?:final\s*)?               # Optional final
        (?:noexcept\s*)?            # Optional noexcept
        \s*\{                       # Opening brace
    '''
    
    # Find all function-like patterns
    for match in re.finditer(pattern, content, re.VERBOSE | re.MULTILINE):
        # Find the position of the opening brace
        brace_pos = content.find('{', match.start())
        if brace_pos != -1:
            func_name = match.group(1)
            functions.append((match.start(), brace_pos, func_name))
    
    return functions

def add_trace_scopes(content: str) -> str:
    """Add TRACE_SCOPE() to all function bodies."""
    
    functions = find_function_bodies(content)
    
    if not functions:
        print("No functions found to instrument")
        return content
    
    # Process in reverse order to preserve positions
    result = content
    added_count = 0
    
    for start_pos, brace_pos, func_name in reversed(functions):
        # Find the position right after the opening brace
        insert_pos = brace_pos + 1
        
        # Check if TRACE_SCOPE already exists in this function
        # Look ahead a few characters to see if it's already there
        lookahead = result[insert_pos:insert_pos+200]
        if 'TRACE_SCOPE()' in lookahead.split('\n')[0:3]:
            print(f"  Skipping {func_name} (already has TRACE_SCOPE)")
            continue
        
        # Determine indentation from the line after the opening brace
        after_brace = result[insert_pos:]
        next_line_match = re.search(r'\n(\s*)\S', after_brace)
        if next_line_match:
            # Use the indentation of the next non-empty line
            indent = next_line_match.group(1)
        else:
            # Default to 4 spaces
            indent = '    '
        
        # Insert TRACE_SCOPE() with proper indentation
        insertion = f"\n{indent}TRACE_SCOPE();"
        result = result[:insert_pos] + insertion + result[insert_pos:]
        added_count += 1
        print(f"  Added TRACE_SCOPE() to {func_name}")
    
    print(f"\nAdded {added_count} TRACE_SCOPE() calls")
    return result

def remove_trace_scopes(content: str) -> str:
    """Remove all TRACE_SCOPE() calls from the file."""
    
    # Pattern to match TRACE_SCOPE() with surrounding whitespace
    pattern = r'^\s*TRACE_SCOPE\(\);\s*$'
    
    lines = content.split('\n')
    result_lines = []
    removed_count = 0
    
    for line in lines:
        if re.match(pattern, line):
            removed_count += 1
            continue  # Skip this line
        result_lines.append(line)
    
    print(f"Removed {removed_count} TRACE_SCOPE() calls")
    return '\n'.join(result_lines)

def process_file(filepath: str, action: str) -> bool:
    """Process a single file: add or remove TRACE_SCOPE()."""
    
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        return False
    
    # Read original content
    with open(filepath, 'r', encoding='utf-8') as f:
        original = f.read()
    
    # Create backup
    backup_path = filepath + '.bak'
    with open(backup_path, 'w', encoding='utf-8') as f:
        f.write(original)
    print(f"Created backup: {backup_path}")
    
    # Process
    if action == 'add':
        result = add_trace_scopes(original)
    elif action == 'remove':
        result = remove_trace_scopes(original)
    else:
        print(f"Error: Unknown action '{action}'")
        return False
    
    # Write result
    if result != original:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(result)
        print(f"\nUpdated: {filepath}")
        return True
    else:
        print(f"\nNo changes needed for: {filepath}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description='Automatically add or remove TRACE_SCOPE() macros in C++ files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Add TRACE_SCOPE() to all functions:
  python trace_instrument.py add myfile.cpp
  
  # Remove all TRACE_SCOPE() calls:
  python trace_instrument.py remove myfile.cpp
  
  # Process multiple files:
  python trace_instrument.py add src/*.cpp
  
Notes:
  - Creates .bak backup files before modifying
  - Skips functions that already have TRACE_SCOPE()
  - Uses simple regex parsing (may need manual adjustment for complex code)
        '''
    )
    
    parser.add_argument('action', choices=['add', 'remove'],
                       help='Action to perform: add or remove TRACE_SCOPE()')
    parser.add_argument('files', nargs='+',
                       help='C++ source files to process')
    
    args = parser.parse_args()
    
    # Process each file
    success_count = 0
    for filepath in args.files:
        print(f"\n{'='*60}")
        print(f"Processing: {filepath}")
        print('='*60)
        if process_file(filepath, args.action):
            success_count += 1
    
    print(f"\n{'='*60}")
    print(f"Processed {success_count} of {len(args.files)} files successfully")
    print('='*60)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(1)

