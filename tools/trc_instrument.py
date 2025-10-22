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

# Printable types for TRACE_ARG value display
PRINTABLE_TYPES = {
    'int', 'long', 'short', 'char', 'bool',
    'unsigned', 'signed', 'size_t', 
    'uint8_t', 'int8_t', 'uint16_t', 'int16_t',
    'uint32_t', 'int32_t', 'uint64_t', 'int64_t',
    'float', 'double', 'long double',
    'std::string', 'std::string_view', 'string', 'string_view'
}

CONTAINER_TYPES = {
    'std::vector', 'std::array', 'std::deque', 
    'std::list', 'std::set', 'vector', 'array', 'deque', 'list', 'set'
}

def parse_function_parameters(signature: str) -> List[Tuple[str, str]]:
    """
    Parse function parameters from signature.
    
    Returns list of (param_name, param_type) tuples.
    """
    # Find parameter list between parentheses
    paren_match = re.search(r'\((.*?)\)(?:\s*(?:const|override|final|noexcept|->.*?))?\s*\{', signature)
    if not paren_match:
        return []
    
    param_str = paren_match.group(1).strip()
    if not param_str or param_str == 'void':
        return []
    
    params = []
    # Split by comma, but respect angle brackets and nested parens
    depth = 0
    current_param = ""
    for char in param_str:
        if char in '<([':
            depth += 1
        elif char in '>)]':
            depth -= 1
        elif char == ',' and depth == 0:
            if current_param.strip():
                params.append(current_param.strip())
            current_param = ""
            continue
        current_param += char
    
    if current_param.strip():
        params.append(current_param.strip())
    
    # Parse each parameter to extract name and type
    result = []
    for param in params:
        # Remove default values (e.g., "int x = 10" -> "int x")
        param = re.sub(r'=.*$', '', param).strip()
        
        # Match pattern: type name or type* name or type& name
        # Handle complex types like: const std::vector<int>& vec
        match = re.match(r'^(.+?)\s+([*&]*\s*\w+)$', param)
        if match:
            param_type = match.group(1).strip()
            param_name = match.group(2).strip()
            # Clean up param_name (remove *, &)
            param_name = param_name.lstrip('*&').strip()
            result.append((param_name, param_type))
    
    return result

def is_printable_type(param_type: str) -> bool:
    """
    Check if a type should have its value printed.
    
    Returns True for basic types, strings, and pointers.
    """
    # Remove const, &, and extra spaces
    clean_type = param_type.replace('const', '').replace('&', '').strip()
    
    # Pointers are always printable (show address)
    if '*' in param_type:
        return True
    
    # Check if it's in our known printable types
    # Also check without std:: prefix
    base_type = clean_type.split('<')[0].strip()  # Get base type before template args
    base_type_no_std = base_type.replace('std::', '')
    
    return base_type in PRINTABLE_TYPES or base_type_no_std in PRINTABLE_TYPES

def is_printable_container(param_type: str) -> Tuple[bool, str]:
    """
    Check if type is a container with printable elements.
    
    Returns: (is_printable_container, element_type)
    """
    # Match patterns like: std::vector<int>, array<string, 10>
    match = re.match(r'(?:const\s+)?(?:std::)?(vector|array|deque|list|set)\s*<\s*([^,>]+)(?:\s*,\s*\d+\s*)?>', param_type)
    if match:
        container_type = match.group(1)
        element_type = match.group(2).strip()
        
        # Check if element type is printable
        if is_printable_type(element_type):
            return True, element_type
    
    return False, ""

def generate_trace_arg(param_name: str, param_type: str, indent: str) -> str:
    """
    Generate appropriate TRACE_ARG() line for a parameter.
    
    Returns formatted TRACE_ARG line with proper indentation.
    """
    # Check if it's a printable container
    is_container, elem_type = is_printable_container(param_type)
    if is_container:
        # Use TRACE_CONTAINER helper
        return f'{indent}TRACE_ARG("{param_name}", {param_type}, TRACE_CONTAINER({param_name}, 5));'
    
    # Check if it's a simple printable type
    elif is_printable_type(param_type):
        # Include value
        return f'{indent}TRACE_ARG("{param_name}", {param_type}, {param_name});'
    
    else:
        # Non-printable type, no value
        return f'{indent}TRACE_ARG("{param_name}", {param_type});'

def find_function_bodies(content: str, verbose: bool = False) -> List[Tuple[int, int, str, str]]:
    """
    Find function body positions in the source code.
    
    Returns list of (start_pos, open_brace_pos, function_name, full_signature) tuples.
    
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
        
        # Capture the full function signature for parameter parsing
        full_signature = match.group(0)
        functions.append((match.start(), brace_pos, func_name, full_signature))
    
    return functions

def add_instrumentation(content: str, add_args: bool = True, verbose: bool = False) -> str:
    """
    Add TRACE_SCOPE() and optionally TRACE_ARG() to all function bodies.
    
    Args:
        content: Source code content
        add_args: If True, add TRACE_ARG() for parameters (default)
        verbose: Show detailed processing info
    """
    functions = find_function_bodies(content, verbose)
    
    if not functions:
        print("No functions found to instrument")
        return content
    
    # Process in reverse order to preserve positions
    result = content
    added_scope_count = 0
    added_args_count = 0
    skipped_count = 0
    
    for start_pos, brace_pos, func_name, full_signature in reversed(functions):
        # Find the position right after the opening brace
        insert_pos = brace_pos + 1
        
        # Check if TRACE_SCOPE already exists in this function
        after_brace = result[insert_pos:insert_pos+300]
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
            indent = next_line_match.group(1)
        else:
            indent = '    '
        
        # Build insertion string: TRACE_SCOPE() and optionally TRACE_ARG()
        insertions = [f"{indent}TRACE_SCOPE();"]
        
        # Add TRACE_ARG() for parameters if requested
        if add_args:
            params = parse_function_parameters(full_signature)
            for param_name, param_type in params:
                trace_arg_line = generate_trace_arg(param_name, param_type, indent)
                insertions.append(trace_arg_line)
                added_args_count += 1
        
        # Combine all insertions
        insertion = "\n" + "\n".join(insertions)
        result = result[:insert_pos] + insertion + result[insert_pos:]
        added_scope_count += 1
        
        if add_args and params:
            print(f"  Added TRACE_SCOPE() and {len(params)} TRACE_ARG() to {func_name}")
        else:
            print(f"  Added TRACE_SCOPE() to {func_name}")
    
    print(f"\nAdded {added_scope_count} TRACE_SCOPE() calls")
    if add_args and added_args_count > 0:
        print(f"Added {added_args_count} TRACE_ARG() calls")
    if skipped_count > 0:
        print(f"Skipped {skipped_count} already instrumented functions")
    return result

# Backward compatibility alias
def add_trace_scopes(content: str, verbose: bool = False) -> str:
    """Add TRACE_SCOPE() to all function bodies (no arguments)."""
    return add_instrumentation(content, add_args=False, verbose=verbose)

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

def remove_trace_args(content: str, verbose: bool = False) -> str:
    """Remove all TRACE_ARG() calls from the file."""
    
    # Pattern to match TRACE_ARG() with surrounding whitespace
    # Matches: TRACE_ARG("name", type) or TRACE_ARG("name", type, value)
    pattern = r'^\s*TRACE_ARG\([^)]+(?:\([^)]*\))?\);\s*$'
    
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
    
    print(f"Removed {removed_count} TRACE_ARG() calls")
    return '\n'.join(result_lines)

def remove_trace_msgs(content: str, verbose: bool = False) -> str:
    """Remove all TRACE_MSG() and TRACE_LOG calls from the file."""
    
    lines = content.split('\n')
    result_lines = []
    removed_msg_count = 0
    removed_log_count = 0
    
    for line in lines:
        # Pattern for TRACE_MSG() - can span multiple arguments
        if re.match(r'^\s*TRACE_MSG\(', line.strip()):
            removed_msg_count += 1
            if verbose:
                print(f"  Removing: {line.strip()}")
            continue  # Skip this line
        
        # Pattern for TRACE_LOG - stream-based logging
        if re.match(r'^\s*TRACE_LOG\s*<<', line.strip()):
            removed_log_count += 1
            if verbose:
                print(f"  Removing: {line.strip()}")
            continue  # Skip this line
        
        result_lines.append(line)
    
    total_removed = removed_msg_count + removed_log_count
    if removed_msg_count > 0:
        print(f"Removed {removed_msg_count} TRACE_MSG() calls")
    if removed_log_count > 0:
        print(f"Removed {removed_log_count} TRACE_LOG calls")
    if total_removed == 0:
        print("No TRACE_MSG() or TRACE_LOG calls found")
    
    return '\n'.join(result_lines)

def remove_all_trace_calls(content: str, verbose: bool = False) -> str:
    """Remove all trace-related calls: TRACE_SCOPE(), TRACE_ARG(), TRACE_MSG(), TRACE_LOG."""
    content = remove_trace_scopes(content, verbose)
    content = remove_trace_args(content, verbose)
    content = remove_trace_msgs(content, verbose)
    return content

def process_file(filepath: str, action: str, no_args: bool = False, remove_all: bool = False, 
                 dry_run: bool = False, verbose: bool = False) -> bool:
    """
    Process a single file: add or remove trace instrumentation.
    
    Args:
        filepath: Path to the C++ source file
        action: Action to perform ('add', 'remove', 'remove-args', 'remove-msgs')
        no_args: For 'add': skip TRACE_ARG() generation
        remove_all: For 'remove': remove all trace calls (SCOPE, ARG, MSG)
        dry_run: Preview changes without modifying file
        verbose: Show detailed processing information
    """
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        return False
    
    # Read original content
    with open(filepath, 'r', encoding='utf-8') as f:
        original = f.read()
    
    # Process based on action
    if action == 'add':
        # By default add both TRACE_SCOPE() and TRACE_ARG()
        result = add_instrumentation(original, add_args=not no_args, verbose=verbose)
    elif action == 'remove':
        if remove_all:
            result = remove_all_trace_calls(original, verbose)
        else:
            result = remove_trace_scopes(original, verbose)
    elif action == 'remove-args':
        result = remove_trace_args(original, verbose)
    elif action == 'remove-msgs':
        result = remove_trace_msgs(original, verbose)
    else:
        print(f"Error: Unknown action '{action}'")
        return False
    
    # Check if changes were made
    if result == original:
        print(f"\nNo changes needed for: {filepath}")
        return False
    
    if dry_run:
        print(f"\n[DRY RUN] Would update: {filepath}")
        scope_diff = result.count('TRACE_SCOPE()') - original.count('TRACE_SCOPE()')
        arg_diff = result.count('TRACE_ARG(') - original.count('TRACE_ARG(')
        if scope_diff != 0:
            print(f"[DRY RUN] TRACE_SCOPE() changes: {scope_diff:+d}")
        if arg_diff != 0:
            print(f"[DRY RUN] TRACE_ARG() changes: {arg_diff:+d}")
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
        description='Automatically add or remove trace instrumentation (TRACE_SCOPE, TRACE_ARG, TRACE_MSG) in C++ files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Add TRACE_SCOPE() and TRACE_ARG() for all parameters (DEFAULT):
  python trc_instrument.py add myfile.cpp
  
  # Add only TRACE_SCOPE() without arguments:
  python trc_instrument.py add --no-args myfile.cpp
  
  # Remove TRACE_SCOPE() only:
  python trc_instrument.py remove myfile.cpp
  
  # Remove all trace calls (SCOPE, ARG, MSG):
  python trc_instrument.py remove --all myfile.cpp
  
  # Remove only TRACE_ARG() calls:
  python trc_instrument.py remove-args myfile.cpp
  
  # Remove only TRACE_MSG() and TRACE_LOG calls:
  python trc_instrument.py remove-msgs myfile.cpp
  
  # Preview changes without modifying:
  python trc_instrument.py add --dry-run myfile.cpp
  
  # Verbose output for debugging:
  python trc_instrument.py add --verbose myfile.cpp
  
  # Process multiple files:
  python trc_instrument.py add src/*.cpp
  
Notes:
  - Default 'add' now includes both TRACE_SCOPE() and TRACE_ARG()
  - Use --no-args to add only TRACE_SCOPE() (old behavior)
  - Creates .bak backup files before modifying (unless --dry-run)
  - Skips functions that already have TRACE_SCOPE()
  - Excludes control flow statements (for, if, while, switch, catch)
  - Container arguments use TRACE_CONTAINER helper (max 5 elements)
  - Uses regex parsing (may need manual adjustment for complex code)
        '''
    )
    
    parser.add_argument('action', choices=['add', 'remove', 'remove-args', 'remove-msgs'],
                       help='Action: add (scope+args), remove (scope), remove-args, remove-msgs')
    parser.add_argument('files', nargs='+',
                       help='C++ source files to process')
    parser.add_argument('--no-args', action='store_true',
                       help='For add: skip TRACE_ARG() and add only TRACE_SCOPE()')
    parser.add_argument('--all', action='store_true',
                       help='For remove: remove all trace calls (SCOPE, ARG, MSG)')
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
        if process_file(filepath, args.action, args.no_args, args.all, args.dry_run, args.verbose):
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
