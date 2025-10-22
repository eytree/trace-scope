#!/usr/bin/env python3
"""
Automatic instrumentation tool for trace_scope.

Usage:
    python trc_instrument.py add <file.cpp>      # Add TRACE_SCOPE() to all functions
    python trc_instrument.py remove <file.cpp>   # Remove all TRACE_SCOPE() calls
    python trc_instrument.py add --dry-run <file.cpp>  # Preview changes without modifying
    python trc_instrument.py --help              # Show help
    
This tool uses regex-based parsing to find function definitions and automatically 
insert or remove TRACE_SCOPE() macros.

Notes:
- Creates backup files (.bak) before modifying (unless --dry-run)
- Excludes control flow statements (for, if, while, switch, catch, etc.)
- Handles most common C++ function patterns
- May need manual adjustment for complex templates or macros
"""

import re
import sys
import os
import argparse
from typing import List, Tuple

# C++ keywords that should NOT be treated as function names
CONTROL_FLOW_KEYWORDS = {
    'for', 'while', 'if', 'switch', 'catch', 'else', 'do',
    'try', 'namespace', 'class', 'struct', 'enum', 'union'
}

def is_control_flow_statement(text_before: str, potential_name: str) -> bool:
    """
    Check if this looks like a control flow statement rather than a function.
    
    Args:
        text_before: Text immediately before the potential function name
        potential_name: The captured name that might be a function
        
    Returns:
        True if this is a control flow statement, False if it's likely a function
    """
    # Check if the name itself is a control flow keyword
    if potential_name in CONTROL_FLOW_KEYWORDS:
        return True
    
    # Look for control flow keywords immediately before the opening paren
    # Extract last few tokens before the match
    tokens_before = re.findall(r'\b\w+\b', text_before[-50:] if len(text_before) > 50 else text_before)
    
    if tokens_before:
        last_token = tokens_before[-1]
        if last_token in CONTROL_FLOW_KEYWORDS:
            return True
    
    return False

def find_function_bodies(content: str, verbose: bool = False) -> List[Tuple[int, int, str]]:
    """
    Find function body positions in the source code.
    
    Returns list of (start_pos, open_brace_pos, function_signature) tuples.
    
    This looks for patterns like:
        return_type function_name(...) {
        return_type ClassName::method_name(...) {
        
    And excludes:
        for (...) {
        if (...) {
        while (...) {
        switch (...) {
        catch (...) {
    """
    functions = []
    
    # Pattern to match function definitions
    # Key improvements:
    # 1. More specific about what can appear before the function name
    # 2. Requires either :: qualifier or a clear return type pattern
    # 3. Better handling of qualifiers
    
    pattern = r'''
        (?:^|\n)                        # Start of line
        [ \t]*                          # Optional whitespace (spaces/tabs only)
        (?:                             # Optional qualifiers/attributes
            (?:template\s*<[^>]+>\s*)?  # Optional template
            (?:inline\s+)?              # Optional inline
            (?:static\s+)?              # Optional static
            (?:virtual\s+)?             # Optional virtual
            (?:constexpr\s+)?           # Optional constexpr
            (?:explicit\s+)?            # Optional explicit
            (?:\[\[[^\]]+\]\]\s*)?      # Optional attributes like [[nodiscard]]
        )?
        (?:                             # Return type or class qualifier
            (?:const\s+)?               # Optional const
            (?:unsigned\s+)?            # Optional unsigned
            (?:signed\s+)?              # Optional signed
            [\w:*&<>,\s]+?              # Return type (can be complex like std::vector<int>)
            [*&]?                       # Optional pointer/reference
            \s+                         # Required space after return type
        )?
        ([\w~]+(?:::[\w~]+)*)           # Function name (possibly qualified with ::, includes ~ for destructors)
        \s*\(                           # Opening parenthesis
        [^)]*                           # Parameters
        \)                              # Closing parenthesis
        \s*                             # Whitespace
        (?:const\s*)?                   # Optional const
        (?:override\s*)?                # Optional override
        (?:final\s*)?                   # Optional final
        (?:noexcept\s*)?                # Optional noexcept
        (?:->[\w\s:*&<>,]+)?            # Optional trailing return type
        \s*\{                           # Opening brace
    '''
    
    # Find all function-like patterns
    for match in re.finditer(pattern, content, re.VERBOSE | re.MULTILINE):
        # Find the position of the opening brace
        brace_pos = content.find('{', match.start())
        if brace_pos == -1:
            continue
            
        func_name = match.group(1)
        
        # Get text before this match to check context
        text_before = content[max(0, match.start() - 100):match.start()]
        
        # Skip if this is a control flow statement
        if is_control_flow_statement(text_before, func_name):
            if verbose:
                print(f"  Skipping control flow: {func_name}")
            continue
        
        # Additional heuristics to filter out false positives
        
        # Skip if the "function name" is actually a keyword
        if func_name in CONTROL_FLOW_KEYWORDS:
            if verbose:
                print(f"  Skipping keyword: {func_name}")
            continue
        
        # Skip lambda expressions (look for ] before the paren)
        text_segment = content[max(0, match.start() - 10):match.start()]
        if ']' in text_segment and '=' in text_segment:
            if verbose:
                print(f"  Skipping lambda expression")
            continue
        
        functions.append((match.start(), brace_pos, func_name))
    
    return functions

def add_trace_scopes(content: str, verbose: bool = False) -> str:
    """Add TRACE_SCOPE() to all function bodies."""
    
    functions = find_function_bodies(content, verbose)
    
    if not functions:
        print("No functions found to instrument")
        return content
    
    # Process in reverse order to preserve positions
    result = content
    added_count = 0
    skipped_count = 0
    
    for start_pos, brace_pos, func_name in reversed(functions):
        # Find the position right after the opening brace
        insert_pos = brace_pos + 1
        
        # Check if TRACE_SCOPE already exists in this function
        # Look at the content right after the opening brace
        after_brace = result[insert_pos:insert_pos+300]
        # Get first few non-empty lines after the brace
        lines_after = [l.strip() for l in after_brace.split('\n')[0:5] if l.strip()]
        if lines_after and 'TRACE_SCOPE()' in lines_after[0]:
            if verbose:
                print(f"  Skipping {func_name} (already has TRACE_SCOPE)")
            skipped_count += 1
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
    if skipped_count > 0:
        print(f"Skipped {skipped_count} already instrumented functions")
    return result

def remove_trace_scopes(content: str, verbose: bool = False) -> str:
    """Remove all TRACE_SCOPE() calls from the file."""
    
    # Pattern to match TRACE_SCOPE() with surrounding whitespace
    pattern = r'^\s*TRACE_SCOPE\(\);\s*$'
    
    lines = content.split('\n')
    result_lines = []
    removed_count = 0
    
    for line in lines:
        if re.match(pattern, line):
            removed_count += 1
            if verbose:
                print(f"  Removing: {line.strip()}")
            continue  # Skip this line
        result_lines.append(line)
    
    print(f"Removed {removed_count} TRACE_SCOPE() calls")
    return '\n'.join(result_lines)

def process_file(filepath: str, action: str, dry_run: bool = False, verbose: bool = False) -> bool:
    """Process a single file: add or remove TRACE_SCOPE()."""
    
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        return False
    
    # Read original content
    with open(filepath, 'r', encoding='utf-8') as f:
        original = f.read()
    
    # Process
    if action == 'add':
        result = add_trace_scopes(original, verbose)
    elif action == 'remove':
        result = remove_trace_scopes(original, verbose)
    else:
        print(f"Error: Unknown action '{action}'")
        return False
    
    # Check if changes were made
    if result == original:
        print(f"\nNo changes needed for: {filepath}")
        return False
    
    if dry_run:
        print(f"\n[DRY RUN] Would update: {filepath}")
        print(f"[DRY RUN] Changes: {result.count('TRACE_SCOPE()') - original.count('TRACE_SCOPE()')} TRACE_SCOPE() calls")
        return True
    
    # Create backup
    backup_path = filepath + '.bak'
    with open(backup_path, 'w', encoding='utf-8') as f:
        f.write(original)
    print(f"Created backup: {backup_path}")
    
    # Write result
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(result)
    print(f"\nUpdated: {filepath}")
    return True

def main():
    parser = argparse.ArgumentParser(
        description='Automatically add or remove TRACE_SCOPE() macros in C++ files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Add TRACE_SCOPE() to all functions:
  python trc_instrument.py add myfile.cpp
  
  # Remove all TRACE_SCOPE() calls:
  python trc_instrument.py remove myfile.cpp
  
  # Preview changes without modifying:
  python trc_instrument.py add --dry-run myfile.cpp
  
  # Verbose output for debugging:
  python trc_instrument.py add --verbose myfile.cpp
  
  # Process multiple files:
  python trc_instrument.py add src/*.cpp
  
Notes:
  - Creates .bak backup files before modifying (unless --dry-run)
  - Skips functions that already have TRACE_SCOPE()
  - Excludes control flow statements (for, if, while, switch, catch)
  - Uses regex parsing (may need manual adjustment for complex code)
        '''
    )
    
    parser.add_argument('action', choices=['add', 'remove'],
                       help='Action to perform: add or remove TRACE_SCOPE()')
    parser.add_argument('files', nargs='+',
                       help='C++ source files to process')
    parser.add_argument('--dry-run', action='store_true',
                       help='Preview changes without modifying files')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Show detailed processing information')
    
    args = parser.parse_args()
    
    if args.dry_run:
        print("=" * 60)
        print("DRY RUN MODE - No files will be modified")
        print("=" * 60)
    
    # Process each file
    success_count = 0
    for filepath in args.files:
        print(f"\n{'='*60}")
        print(f"Processing: {filepath}")
        print('='*60)
        if process_file(filepath, args.action, args.dry_run, args.verbose):
            success_count += 1
    
    print(f"\n{'='*60}")
    if args.dry_run:
        print(f"[DRY RUN] Would process {success_count} of {len(args.files)} files")
    else:
        print(f"Processed {success_count} of {len(args.files)} files successfully")
    print('='*60)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(1)
