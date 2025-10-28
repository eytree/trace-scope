#!/usr/bin/env python3
"""
trace-scope: All-in-one tool for C++ tracing instrumentation and analysis

Commands:
  instrument    Add/remove TRC_SCOPE() and TRC_ARG() macros
  analyze       Display and analyze trace files
  stats         Generate performance statistics
  callgraph     Generate call graphs
  compare       Compare trace performance
  diff          Diff two trace files

Usage:
  python trc.py instrument add file.cpp
  python trc.py analyze trace.bin
  python trc.py stats trace.bin --sort total
  python trc.py callgraph trace.bin --format dot
  python trc.py compare trace1.bin trace2.bin
  python trc.py diff trace1.bin trace2.bin
"""

import sys
import argparse
import os
import glob
import re
import struct
import pathlib
import tempfile
from typing import List, Tuple
from collections import defaultdict

# ============================================================================
# SECTION 1: Common Utilities (from trc_common.py)
# ============================================================================

# Version information - read from VERSION file at project root
def get_version():
    """Read version from VERSION file at project root."""
    version_file = pathlib.Path(__file__).parent.parent / 'VERSION'
    if version_file.exists():
        return version_file.read_text().strip()
    return 'unknown'

__version__ = get_version()

# Event types (must match C++ EventType enum)
EVENT_TYPE_ENTER = 0
EVENT_TYPE_EXIT = 1
EVENT_TYPE_MSG = 2

# ANSI color codes (matches C++ colors)
COLORS = [
    '\033[31m',  # Red
    '\033[32m',  # Green
    '\033[33m',  # Yellow
    '\033[34m',  # Blue
    '\033[35m',  # Magenta
    '\033[36m',  # Cyan
    '\033[37m',  # White
    '\033[91m',  # Bright Red
]
RESET = '\033[0m'

# === Binary Format Reading ===

def readn(f, n):
    """Read exactly n bytes or raise EOFError."""
    b = f.read(n)
    if len(b) < n:
        raise EOFError
    return b

def read_str(f):
    """Read length-prefixed string (2-byte length + string)."""
    (n,) = struct.unpack('<H', readn(f, 2))
    return readn(f, n).decode('utf-8', errors='replace') if n > 0 else ''

def read_header(f):
    """
    Read trace file header and return version.
    
    Returns:
        version (int): Binary format version (1 or 2)
    
    Raises:
        ValueError: If magic or version is invalid
    """
    magic = readn(f, 8)
    if magic != b'TRCLOG10':
        raise ValueError(f'Bad magic (expected TRCLOG10, got {magic})')
    
    (version, padding) = struct.unpack('<II', readn(f, 8))
    if version < 1 or version > 2:
        raise ValueError(f'Unsupported version {version} (expected 1 or 2)')
    
    return version

def read_event(f, version):
    """
    Read a single event based on format version.
    
    Args:
        f: File object to read from
        version: Binary format version (1 or 2)
    
    Returns:
        dict: Event with keys: type, tid, color_offset, ts_ns, depth,
              dur_ns, memory_rss, file, func, msg, line
    """
    (typ,) = struct.unpack('<B', readn(f, 1))
    (tid,) = struct.unpack('<I', readn(f, 4))
    
    if version >= 2:
        (color_offset,) = struct.unpack('<B', readn(f, 1))
    else:
        color_offset = 0  # Default for version 1
    
    (ts_ns,) = struct.unpack('<Q', readn(f, 8))
    (depth,) = struct.unpack('<I', readn(f, 4))
    (dur_ns,) = struct.unpack('<Q', readn(f, 8))
    
    # Read memory_rss field (added in version 2)
    if version >= 2:
        (memory_rss,) = struct.unpack('<Q', readn(f, 8))
    else:
        memory_rss = 0  # Default for version 1
    
    file = read_str(f)
    func = read_str(f)
    msg = read_str(f)
    
    (line,) = struct.unpack('<I', readn(f, 4))
    
    return {
        'type': typ,
        'tid': tid,
        'color_offset': color_offset,
        'ts_ns': ts_ns,
        'depth': depth,
        'dur_ns': dur_ns,
        'memory_rss': memory_rss,
        'file': file,
        'func': func,
        'msg': msg,
        'line': line
    }

def read_all_events(filename):
    """
    Read all events from a trace file.
    
    Args:
        filename: Path to binary trace file
    
    Returns:
        tuple: (version, events_list)
    """
    events = []
    with open(filename, 'rb') as f:
        version = read_header(f)
        try:
            while True:
                event = read_event(f, version)
                events.append(event)
        except EOFError:
            pass
    return version, events

# === Filtering ===

def wildcard_match(pattern, text):
    """Simple wildcard matching (* matches zero or more chars)."""
    if not pattern:
        return False
    if text is None:
        return False
    
    # Convert wildcard to regex: escape special chars, replace * with .*
    regex = '^' + re.escape(pattern).replace(r'\*', '.*') + '$'
    return bool(re.match(regex, text))

def matches_any(text, patterns):
    """Check if text matches any pattern in list."""
    if not text or not patterns:
        return False
    return any(wildcard_match(p, text) for p in patterns)

class EventFilter:
    """Filter events by function, file, depth, and thread."""
    
    def __init__(self):
        self.include_functions = []
        self.exclude_functions = []
        self.include_files = []
        self.exclude_files = []
        self.include_threads = []
        self.exclude_threads = []
        self.max_depth = -1
    
    def should_trace(self, event):
        """Check if event passes all filters (matches C++ logic)."""
        # Check depth filter
        if self.max_depth >= 0 and event['depth'] > self.max_depth:
            return False
        
        # Check function filters
        func = event['func']
        if func:
            if matches_any(func, self.exclude_functions):
                return False
            if self.include_functions and not matches_any(func, self.include_functions):
                return False
        
        # Check file filters
        file = event['file']
        if file:
            if matches_any(file, self.exclude_files):
                return False
            if self.include_files and not matches_any(file, self.include_files):
                return False
        
        # Check thread filters
        tid = event['tid']
        if self.exclude_threads and tid in self.exclude_threads:
            return False
        if self.include_threads and tid not in self.include_threads:
            return False
        
        return True

# === Formatting ===

def format_duration(dur_ns):
    """Format duration with auto-scaled units matching C++ output."""
    if dur_ns < 1000:
        return f'{dur_ns} ns'
    elif dur_ns < 1000000:
        return f'{dur_ns/1000:.2f} us'
    elif dur_ns < 1000000000:
        return f'{dur_ns/1000000:.2f} ms'
    else:
        return f'{dur_ns/1000000000:.3f} s'

def format_memory(bytes_val):
    """Format memory size with auto-scaled units."""
    if bytes_val < 1024:
        return f'{bytes_val} B'
    elif bytes_val < 1024 * 1024:
        return f'{bytes_val/1024:.2f} KB'
    elif bytes_val < 1024 * 1024 * 1024:
        return f'{bytes_val/(1024*1024):.2f} MB'
    else:
        return f'{bytes_val/(1024*1024*1024):.2f} GB'

def get_color(event, use_color):
    """Get ANSI color code for event (thread-aware)."""
    if not use_color:
        return '', ''
    
    color_idx = (event['depth'] + event['color_offset']) % 8
    return COLORS[color_idx], RESET

# === Statistics ===

def compute_stats(events, filter_obj):
    """
    Compute performance statistics from events.
    
    Args:
        events: List of event dictionaries
        filter_obj: EventFilter instance
    
    Returns:
        tuple: (global_stats, thread_stats)
            global_stats: dict of func_name -> {calls, total_ns, min_ns, max_ns, avg_ns, memory_delta}
            thread_stats: dict of tid -> {func_name -> {calls, total_ns, min_ns, max_ns, avg_ns, memory_delta}}
    """
    # Apply filters first
    filtered_events = [e for e in events if filter_obj.should_trace(e)]
    
    # Global stats: func_name -> {calls, total, min, max, memory}
    global_stats = {}
    
    # Per-thread stats: tid -> {func_name -> {calls, total, min, max, memory}}
    thread_stats = {}
    
    for event in filtered_events:
        # Only count Exit events (have duration)
        if event['type'] != EVENT_TYPE_EXIT:
            continue
        
        func = event['func']
        if not func:
            continue
        
        dur = event['dur_ns']
        tid = event['tid']
        memory = event.get('memory_rss', 0)
        
        # Update global stats
        if func not in global_stats:
            global_stats[func] = {
                'calls': 0,
                'total_ns': 0,
                'min_ns': float('inf'),
                'max_ns': 0,
                'memory_delta': 0
            }
        
        global_stats[func]['calls'] += 1
        global_stats[func]['total_ns'] += dur
        global_stats[func]['min_ns'] = min(global_stats[func]['min_ns'], dur)
        global_stats[func]['max_ns'] = max(global_stats[func]['max_ns'], dur)
        global_stats[func]['memory_delta'] = max(global_stats[func]['memory_delta'], memory)
        
        # Update per-thread stats
        if tid not in thread_stats:
            thread_stats[tid] = {}
        
        if func not in thread_stats[tid]:
            thread_stats[tid][func] = {
                'calls': 0,
                'total_ns': 0,
                'min_ns': float('inf'),
                'max_ns': 0,
                'memory_delta': 0
            }
        
        thread_stats[tid][func]['calls'] += 1
        thread_stats[tid][func]['total_ns'] += dur
        thread_stats[tid][func]['min_ns'] = min(thread_stats[tid][func]['min_ns'], dur)
        thread_stats[tid][func]['max_ns'] = max(thread_stats[tid][func]['max_ns'], dur)
        thread_stats[tid][func]['memory_delta'] = max(thread_stats[tid][func]['memory_delta'], memory)
    
    # Compute averages
    for stats in global_stats.values():
        stats['avg_ns'] = stats['total_ns'] / stats['calls'] if stats['calls'] > 0 else 0
    
    for tid_stats in thread_stats.values():
        for stats in tid_stats.values():
            stats['avg_ns'] = stats['total_ns'] / stats['calls'] if stats['calls'] > 0 else 0
    
    return global_stats, thread_stats

def print_stats_table(global_stats, thread_stats, sort_by='total'):
    """Print statistics as formatted table."""
    print("\n" + "=" * 100)
    print(" Performance Metrics Summary")
    print("=" * 100)

    # Sort global stats
    sort_key = {
        'total': lambda x: x[1]['total_ns'],
        'calls': lambda x: x[1]['calls'],
        'avg': lambda x: x[1]['avg_ns'],
        'name': lambda x: x[0]
    }[sort_by]

    sorted_global = sorted(global_stats.items(), key=sort_key, reverse=(sort_by != 'name'))

    # Print global table
    print("\nGlobal Statistics:")
    print("-" * 100)
    print(f"{'Function':<50} {'Calls':>10} {'Total':>12} {'Avg':>12} {'Min':>12} {'Max':>12} {'Memory':>12}")
    print("-" * 100)

    for func, stats in sorted_global:
        print(f"{func:<50} {stats['calls']:>10} "
              f"{format_duration(stats['total_ns']):>12} "
              f"{format_duration(stats['avg_ns']):>12} "
              f"{format_duration(stats['min_ns']):>12} "
              f"{format_duration(stats['max_ns']):>12} "
              f"{format_memory(stats['memory_delta']):>12}")

    # Per-thread breakdown (if multiple threads)
    if len(thread_stats) > 1:
        print("\nPer-Thread Breakdown:")
        print("=" * 100)

        for tid, tid_stats in sorted(thread_stats.items()):
            total_calls = sum(s['calls'] for s in tid_stats.values())
            print(f"\nThread 0x{tid:08x} ({total_calls} calls):")
            print("-" * 100)
            print(f"{'Function':<50} {'Calls':>10} {'Total':>12} {'Avg':>12} {'Memory':>12}")
            print("-" * 100)

            sorted_thread = sorted(tid_stats.items(), key=lambda x: x[1]['total_ns'], reverse=True)
            for func, stats in sorted_thread:
                print(f"{func:<50} {stats['calls']:>10} "
                      f"{format_duration(stats['total_ns']):>12} "
                      f"{format_duration(stats['avg_ns']):>12} "
                      f"{format_memory(stats['memory_delta']):>12}")

    print("=" * 100 + "\n")

def export_csv(global_stats, thread_stats, filename):
    """Export statistics to CSV file."""
    import csv
    
    with open(filename, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        # Global stats
        writer.writerow(['Scope', 'Function', 'Calls', 'Total_ns', 'Avg_ns', 'Min_ns', 'Max_ns', 'Memory_delta'])
        
        for func, stats in global_stats.items():
            writer.writerow([
                'Global',
                func,
                stats['calls'],
                stats['total_ns'],
                int(stats['avg_ns']),
                stats['min_ns'],
                stats['max_ns'],
                stats['memory_delta']
            ])
        
        # Per-thread stats
        for tid, tid_stats in thread_stats.items():
            for func, stats in tid_stats.items():
                writer.writerow([
                    f'Thread_0x{tid:08x}',
                    func,
                    stats['calls'],
                    stats['total_ns'],
                    int(stats['avg_ns']),
                    stats['min_ns'],
                    stats['max_ns'],
                    stats['memory_delta']
                ])
    
    print(f"Statistics exported to {filename}")

def export_json(global_stats, thread_stats, filename):
    """Export statistics to JSON file."""
    import json
    
    # Convert to JSON-serializable format
    data = {
        'global': {func: {
            'calls': stats['calls'],
            'total_ns': stats['total_ns'],
            'avg_ns': stats['avg_ns'],
            'min_ns': stats['min_ns'],
            'max_ns': stats['max_ns'],
            'memory_delta': stats['memory_delta']
        } for func, stats in global_stats.items()},
        'threads': {f'0x{tid:08x}': {func: {
            'calls': stats['calls'],
            'total_ns': stats['total_ns'],
            'avg_ns': stats['avg_ns'],
            'min_ns': stats['min_ns'],
            'max_ns': stats['max_ns'],
            'memory_delta': stats['memory_delta']
        } for func, stats in tid_stats.items()} for tid, tid_stats in thread_stats.items()}
    }
    
    with open(filename, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    
    print(f"Statistics exported to {filename}")

# ============================================================================
# SECTION 2: Instrumentation (from trc_instrument.py)
# ============================================================================

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

# Printable types for TRC_ARG value display
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
    Generate appropriate TRC_ARG() line for a parameter.
    
    Returns formatted TRC_ARG line with proper indentation.
    """
    # Check if it's a printable container
    is_container, elem_type = is_printable_container(param_type)
    if is_container:
        # Use TRC_CONTAINER helper
        return f'{indent}TRC_ARG("{param_name}", {param_type}, TRC_CONTAINER({param_name}, 5));'
    
    # Check if it's a simple printable type
    elif is_printable_type(param_type):
        # Include value
        return f'{indent}TRC_ARG("{param_name}", {param_type}, {param_name});'
    
    else:
        # Non-printable type, no value
        return f'{indent}TRC_ARG("{param_name}", {param_type});'

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
            (?:signed\s+)?             # Optional signed
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
    Add TRC_SCOPE() and optionally TRC_ARG() to all function bodies.
    
    Args:
        content: Source code content
        add_args: If True, add TRC_ARG() for parameters (default)
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
        
        # Check if TRC_SCOPE already exists in this function
        after_brace = result[insert_pos:insert_pos+300]
        lines_after = [l.strip() for l in after_brace.split('\n')[0:5] if l.strip()]
        if lines_after and 'TRC_SCOPE()' in lines_after[0]:
            if verbose:
                print(f"  Skipping {func_name} (already has TRC_SCOPE)")
            skipped_count += 1
            continue
        
        # Determine indentation from the line after the opening brace
        after_brace = result[insert_pos:]
        next_line_match = re.search(r'\n(\s*)\S', after_brace)
        if next_line_match:
            indent = next_line_match.group(1)
        else:
            indent = '    '
        
        # Build insertion string: TRC_SCOPE() and optionally TRC_ARG()
        insertions = [f"{indent}TRC_SCOPE();"]
        
        # Add TRC_ARG() for parameters if requested
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
            print(f"  Added TRC_SCOPE() and {len(params)} TRC_ARG() to {func_name}")
        else:
            print(f"  Added TRC_SCOPE() to {func_name}")
    
    print(f"\nAdded {added_scope_count} TRC_SCOPE() calls")
    if add_args and added_args_count > 0:
        print(f"Added {added_args_count} TRC_ARG() calls")
    if skipped_count > 0:
        print(f"Skipped {skipped_count} already instrumented functions")
    return result

# Backward compatibility alias
def add_trace_scopes(content: str, verbose: bool = False) -> str:
    """Add TRC_SCOPE() to all function bodies (no arguments)."""
    return add_instrumentation(content, add_args=False, verbose=verbose)

def remove_trace_scopes(content: str, verbose: bool = False) -> str:
    """Remove all TRC_SCOPE() calls from the file."""
    
    # Pattern to match TRC_SCOPE() with surrounding whitespace
    pattern = r'^\s*TRC_SCOPE\(\);\s*$'
    
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
    
    print(f"Removed {removed_count} TRC_SCOPE() calls")
    return '\n'.join(result_lines)

def remove_trace_args(content: str, verbose: bool = False) -> str:
    """Remove all TRC_ARG() calls from the file."""
    
    # Pattern to match TRC_ARG() with surrounding whitespace
    # Matches: TRC_ARG("name", type) or TRC_ARG("name", type, value)
    pattern = r'^\s*TRC_ARG\([^)]+(?:\([^)]*\))?\);\s*$'
    
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
    
    print(f"Removed {removed_count} TRC_ARG() calls")
    return '\n'.join(result_lines)

def remove_trace_msgs(content: str, verbose: bool = False) -> str:
    """Remove all TRC_MSG() and TRC_LOG calls from the file."""
    
    lines = content.split('\n')
    result_lines = []
    removed_msg_count = 0
    removed_log_count = 0
    
    for line in lines:
        # Pattern for TRC_MSG() - can span multiple arguments
        if re.match(r'^\s*TRC_MSG\(', line.strip()):
            removed_msg_count += 1
            if verbose:
                print(f"  Removing: {line.strip()}")
            continue  # Skip this line
        
        # Pattern for TRC_LOG - stream-based logging
        if re.match(r'^\s*TRC_LOG\s*<<', line.strip()):
            removed_log_count += 1
            if verbose:
                print(f"  Removing: {line.strip()}")
            continue  # Skip this line
        
        result_lines.append(line)
    
    total_removed = removed_msg_count + removed_log_count
    if removed_msg_count > 0:
        print(f"Removed {removed_msg_count} TRC_MSG() calls")
    if removed_log_count > 0:
        print(f"Removed {removed_log_count} TRC_LOG calls")
    if total_removed == 0:
        print("No TRC_MSG() or TRC_LOG calls found")
    
    return '\n'.join(result_lines)

def remove_all_trace_calls(content: str, verbose: bool = False) -> str:
    """Remove all trace-related calls: TRC_SCOPE(), TRC_ARG(), TRC_MSG(), TRC_LOG."""
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
        no_args: For 'add': skip TRC_ARG() generation
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
        # By default add both TRC_SCOPE() and TRC_ARG()
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
        scope_diff = result.count('TRC_SCOPE()') - original.count('TRC_SCOPE()')
        arg_diff = result.count('TRC_ARG(') - original.count('TRC_ARG(')
        if scope_diff != 0:
            print(f"[DRY RUN] TRC_SCOPE() changes: {scope_diff:+d}")
        if arg_diff != 0:
            print(f"[DRY RUN] TRC_ARG() changes: {arg_diff:+d}")
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

# ============================================================================
# SECTION 3: Analysis (from trc_analyze.py)
# ============================================================================

def process_directory(path, pattern='*.trc', recursive=False, sort_by='chronological'):
    """
    Find and sort trace files in a directory.
    
    Args:
        path: Directory path to search
        pattern: File pattern to match (default: *.trc)
        recursive: Search subdirectories
        sort_by: Sort method ('chronological', 'name', 'size')
    
    Returns:
        list: Sorted list of trace file paths
    """
    if not os.path.isdir(path):
        return []
    
    # Find files
    if recursive:
        files = glob.glob(os.path.join(path, '**', pattern), recursive=True)
    else:
        files = glob.glob(os.path.join(path, pattern))
    
    # Sort files
    if sort_by == 'chronological':
        files.sort(key=lambda f: os.path.getmtime(f), reverse=True)
    elif sort_by == 'name':
        files.sort()
    elif sort_by == 'size':
        files.sort(key=lambda f: os.path.getsize(f), reverse=True)
    
    return files

def format_event_line(event, use_color=True, show_timestamp=True, show_timing=True):
    """
    Format a single event as a line of output.
    
    Args:
        event: Event dictionary
        use_color: Enable ANSI color codes
        show_timestamp: Show timestamp
        show_timing: Show timing information
    
    Returns:
        str: Formatted line
    """
    # Get color codes
    color_start, color_end = get_color(event, use_color)
    
    # Build the line
    parts = []
    
    # Thread ID and depth
    parts.append(f"({event['tid']:08x})")
    
    # Depth indentation
    indent = "  " * event['depth']
    parts.append(indent)
    
    # Event type and function
    if event['type'] == EVENT_TYPE_ENTER:
        parts.append(f"-> {event['func']}")
    elif event['type'] == EVENT_TYPE_EXIT:
        parts.append(f"<- {event['func']}")
    elif event['type'] == EVENT_TYPE_MSG:
        parts.append(f"- {event['msg']}")
    
    # Add message if present
    if event['msg'] and event['type'] != EVENT_TYPE_MSG:
        parts.append(f" | {event['msg']}")
    
    # Add timing for exit events
    if event['type'] == EVENT_TYPE_EXIT and show_timing and event['dur_ns'] > 0:
        parts.append(f" [{format_duration(event['dur_ns'])}]")
    
    # Add timestamp
    if show_timestamp:
        ts_sec = event['ts_ns'] / 1e9
        parts.append(f" @ {ts_sec:.6f}s")
    
    # Combine and colorize
    line = "".join(parts)
    return f"{color_start}{line}{color_end}"

def display_trace(filename, filter_obj=None, use_color=True, show_timestamp=True, show_timing=True):
    """
    Display trace events from a file.
    
    Args:
        filename: Path to trace file
        filter_obj: EventFilter instance (optional)
        use_color: Enable ANSI color codes
        show_timestamp: Show timestamps
        show_timing: Show timing information
    """
    try:
        version, events = read_all_events(filename)
        print(f"Loaded {len(events)} events from {filename} (format version {version})")
        
        if not events:
            print("No events found")
            return
        
        # Apply filter if provided
        if filter_obj:
            events = [e for e in events if filter_obj.should_trace(e)]
            print(f"After filtering: {len(events)} events")
        
        # Display events
        for event in events:
            line = format_event_line(event, use_color, show_timestamp, show_timing)
            print(line)
            
    except Exception as e:
        print(f"Error reading trace file: {e}")

# ============================================================================
# SECTION 4: Call Graph (from trc_callgraph.py)
# ============================================================================

class CallGraphNode:
    """Represents a node in the call graph."""
    
    def __init__(self, func_name):
        self.func_name = func_name
        self.call_count = 0
        self.total_duration = 0
        self.callees = defaultdict(lambda: {'count': 0, 'duration': 0})
        self.callers = defaultdict(lambda: {'count': 0, 'duration': 0})
    
    def add_call_to(self, callee, duration=0):
        """Record a call from this function to callee."""
        self.callees[callee]['count'] += 1
        self.callees[callee]['duration'] += duration
    
    def add_call_from(self, caller, duration=0):
        """Record a call to this function from caller."""
        self.callers[caller]['count'] += 1
        self.callers[caller]['duration'] += duration

class CallGraph:
    """Call graph data structure."""
    
    def __init__(self):
        self.nodes = {}  # func_name -> CallGraphNode
        self.root_functions = set()  # Functions with no callers
    
    def get_or_create_node(self, func_name):
        """Get existing node or create new one."""
        if func_name not in self.nodes:
            self.nodes[func_name] = CallGraphNode(func_name)
        return self.nodes[func_name]
    
    def add_edge(self, caller, callee, duration=0):
        """Add an edge from caller to callee."""
        caller_node = self.get_or_create_node(caller)
        callee_node = self.get_or_create_node(callee)
        
        caller_node.add_call_to(callee, duration)
        callee_node.add_call_from(caller, duration)
    
    def finalize(self):
        """Identify root functions (those with no callers)."""
        for func_name, node in self.nodes.items():
            if not node.callers:
                self.root_functions.add(func_name)

def build_call_graph(events, filter_obj=None):
    """
    Build call graph from trace events.
    
    Args:
        events: List of event dictionaries
        filter_obj: Optional EventFilter to apply
    
    Returns:
        CallGraph object
    """
    # Apply filters if provided
    if filter_obj:
        events = [e for e in events if filter_obj.should_trace(e)]
    
    graph = CallGraph()
    call_stack = []  # Stack of (func_name, enter_time)
    
    for event in events:
        if event['type'] == EVENT_TYPE_ENTER:
            # Push onto call stack
            call_stack.append((event['func'], event['ts_ns']))
            
        elif event['type'] == EVENT_TYPE_EXIT:
            if not call_stack:
                continue  # Mismatched exit event
            
            # Pop from call stack
            func_name, enter_time = call_stack.pop()
            duration = event['ts_ns'] - enter_time
            
            # Record call relationship
            if call_stack:  # If there's a caller
                caller = call_stack[-1][0]
                graph.add_edge(caller, func_name, duration)
            else:  # Root function
                graph.get_or_create_node(func_name)
    
    graph.finalize()
    return graph

def print_tree(graph, max_depth=10, min_calls=1):
    """
    Print call graph as a tree structure.
    
    Args:
        graph: CallGraph object
        max_depth: Maximum depth to print
        min_calls: Minimum call count to include
    """
    def print_node(node, depth=0, prefix=""):
        if depth > max_depth:
            return
        
        # Print this node
        indent = "  " * depth
        print(f"{prefix}{indent}{node.func_name} ({node.call_count} calls, {format_duration(node.total_duration)})")
        
        # Print callees
        if depth < max_depth:
            callees = [(name, info) for name, info in node.callees.items() 
                      if info['count'] >= min_calls]
            callees.sort(key=lambda x: x[1]['count'], reverse=True)
            
            for i, (callee_name, info) in enumerate(callees):
                is_last = (i == len(callees) - 1)
                next_prefix = "└─ " if is_last else "├─ "
                callee_node = graph.nodes.get(callee_name)
                if callee_node:
                    print_node(callee_node, depth + 1, next_prefix)
    
    # Start from root functions
    for root_name in sorted(graph.root_functions):
        root_node = graph.nodes[root_name]
        print_node(root_node)

def export_dot(graph, filename):
    """
    Export call graph to GraphViz DOT format.
    
    Args:
        graph: CallGraph object
        filename: Output filename
    """
    with open(filename, 'w') as f:
        f.write("digraph CallGraph {\n")
        f.write("  rankdir=TB;\n")
        f.write("  node [shape=box, style=filled];\n\n")
        
        # Add nodes
        for func_name, node in graph.nodes.items():
            # Color by call count
            if node.call_count > 100:
                color = "red"
            elif node.call_count > 10:
                color = "orange"
            else:
                color = "lightblue"
            
            f.write(f'  "{func_name}" [label="{func_name}\\n{node.call_count} calls", fillcolor="{color}"];\n')
        
        f.write("\n")
        
        # Add edges
        for func_name, node in graph.nodes.items():
            for callee, info in node.callees.items():
                f.write(f'  "{func_name}" -> "{callee}" [label="{info["count"]}"];\n')
        
        f.write("}\n")
    
    print(f"Call graph exported to {filename}")

# ============================================================================
# SECTION 5: Compare (from trc_compare.py)
# ============================================================================

def compare_traces(file1, file2, threshold=0.1):
    """
    Compare two trace files for performance differences.
    
    Args:
        file1: First trace file
        file2: Second trace file
        threshold: Performance difference threshold (0.1 = 10%)
    
    Returns:
        dict: Comparison results
    """
    try:
        _, events1 = read_all_events(file1)
        _, events2 = read_all_events(file2)
        
        # Compute stats for both
        filter1 = EventFilter()
        filter2 = EventFilter()
        
        stats1_global, stats1_thread = compute_stats(events1, filter1)
        stats2_global, stats2_thread = compute_stats(events2, filter2)
        
        # Compare global stats
        comparison = {}
        all_functions = set(stats1_global.keys()) | set(stats2_global.keys())
        
        for func in all_functions:
            stats1 = stats1_global.get(func, {'total_ns': 0, 'calls': 0})
            stats2 = stats2_global.get(func, {'total_ns': 0, 'calls': 0})
            
            if stats1['calls'] > 0 and stats2['calls'] > 0:
                avg1 = stats1['total_ns'] / stats1['calls']
                avg2 = stats2['total_ns'] / stats2['calls']
                
                if avg1 > 0:
                    diff_pct = (avg2 - avg1) / avg1 * 100
                    if abs(diff_pct) > threshold * 100:
                        comparison[func] = {
                            'avg1': avg1,
                            'avg2': avg2,
                            'diff_pct': diff_pct,
                            'calls1': stats1['calls'],
                            'calls2': stats2['calls']
                        }
        
        return comparison
        
    except Exception as e:
        print(f"Error comparing traces: {e}")
        return {}

def print_comparison(comparison):
    """Print comparison results."""
    if not comparison:
        print("No significant differences found")
        return
    
    print("\nPerformance Comparison:")
    print("=" * 80)
    print(f"{'Function':<40} {'Avg1':<12} {'Avg2':<12} {'Diff%':<8} {'Calls1':<8} {'Calls2':<8}")
    print("=" * 80)
    
    for func, data in sorted(comparison.items(), key=lambda x: abs(x[1]['diff_pct']), reverse=True):
        print(f"{func:<40} "
              f"{format_duration(data['avg1']):<12} "
              f"{format_duration(data['avg2']):<12} "
              f"{data['diff_pct']:+.1f}% "
              f"{data['calls1']:<8} "
              f"{data['calls2']:<8}")

# ============================================================================
# SECTION 6: Diff (from trc_diff.py)
# ============================================================================

def diff_traces(file1, file2):
    """
    Compare two trace files for structural differences.
    
    Args:
        file1: First trace file
        file2: Second trace file
    
    Returns:
        dict: Diff results
    """
    try:
        _, events1 = read_all_events(file1)
        _, events2 = read_all_events(file2)
        
        # Extract function call sequences
        seq1 = extract_call_sequence(events1)
        seq2 = extract_call_sequence(events2)
        
        # Find differences
        diff = {
            'only_in_1': [],
            'only_in_2': [],
            'common': [],
            'total_events_1': len(events1),
            'total_events_2': len(events2)
        }
        
        # Find functions only in trace 1
        for func in seq1:
            if func not in seq2:
                diff['only_in_1'].append(func)
        
        # Find functions only in trace 2
        for func in seq2:
            if func not in seq1:
                diff['only_in_2'].append(func)
        
        # Find common functions
        for func in seq1:
            if func in seq2:
                diff['common'].append(func)
        
        return diff
        
    except Exception as e:
        print(f"Error diffing traces: {e}")
        return {}

def extract_call_sequence(events):
    """Extract function call sequence from events."""
    sequence = []
    for event in events:
        if event['type'] == EVENT_TYPE_ENTER:
            sequence.append(event['func'])
    return sequence

def print_diff(diff):
    """Print diff results."""
    print(f"\nTrace Diff Results:")
    print(f"Total events: {diff.get('total_events_1', 0)} vs {diff.get('total_events_2', 0)}")
    print(f"Common functions: {len(diff.get('common', []))}")
    print(f"Only in trace 1: {len(diff.get('only_in_1', []))}")
    print(f"Only in trace 2: {len(diff.get('only_in_2', []))}")
    
    if diff.get('only_in_1'):
        print(f"\nOnly in trace 1: {', '.join(diff['only_in_1'])}")
    if diff.get('only_in_2'):
        print(f"\nOnly in trace 2: {', '.join(diff['only_in_2'])}")

# ============================================================================
# SECTION 7: Main CLI with subcommands
# ============================================================================

def instrument_command(args):
    """Handle instrument subcommand."""
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

def analyze_command(args):
    """Handle analyze subcommand."""
    # Create filter if specified
    filter_obj = None
    if args.function or args.file or args.thread or args.depth >= 0:
        filter_obj = EventFilter()
        if args.function:
            filter_obj.include_functions = args.function
        if args.file:
            filter_obj.include_files = args.file
        if args.thread:
            filter_obj.include_threads = args.thread
        if args.depth >= 0:
            filter_obj.max_depth = args.depth
    
    # Process files
    files = []
    for pattern in args.files:
        if os.path.isfile(pattern):
            files.append(pattern)
        else:
            # Try as directory
            found = process_directory(pattern, args.pattern, args.recursive, args.sort)
            files.extend(found)
    
    if not files:
        print("No trace files found")
        return
    
    # Display traces
    for filename in files:
        print(f"\n{'='*60}")
        print(f"Analyzing: {filename}")
        print('='*60)
        display_trace(filename, filter_obj, not args.no_color, not args.no_timestamp, not args.no_timing)

def stats_command(args):
    """Handle stats subcommand."""
    # Create filter
    filter_obj = EventFilter()
    if args.function:
        filter_obj.include_functions = args.function
    if args.file:
        filter_obj.include_files = args.file
    if args.thread:
        filter_obj.include_threads = args.thread
    if args.depth >= 0:
        filter_obj.max_depth = args.depth
    
    # Process files
    files = []
    for pattern in args.files:
        if os.path.isfile(pattern):
            files.append(pattern)
        else:
            found = process_directory(pattern, args.pattern, args.recursive, args.sort)
            files.extend(found)
    
    if not files:
        print("No trace files found")
        return
    
    # Compute and display stats
    for filename in files:
        print(f"\n{'='*60}")
        print(f"Statistics for: {filename}")
        print('='*60)
        
        try:
            _, events = read_all_events(filename)
            global_stats, thread_stats = compute_stats(events, filter_obj)
            print_stats_table(global_stats, thread_stats, args.sort)
            
            # Export if requested
            if args.csv:
                export_csv(global_stats, thread_stats, args.csv)
            if args.json:
                export_json(global_stats, thread_stats, args.json)
                
        except Exception as e:
            print(f"Error processing {filename}: {e}")

def callgraph_command(args):
    """Handle callgraph subcommand."""
    # Create filter
    filter_obj = EventFilter()
    if args.function:
        filter_obj.include_functions = args.function
    if args.file:
        filter_obj.include_files = args.file
    if args.thread:
        filter_obj.include_threads = args.thread
    if args.depth >= 0:
        filter_obj.max_depth = args.depth
    
    # Process files
    files = []
    for pattern in args.files:
        if os.path.isfile(pattern):
            files.append(pattern)
        else:
            found = process_directory(pattern, args.pattern, args.recursive, args.sort)
            files.extend(found)
    
    if not files:
        print("No trace files found")
        return
    
    # Generate call graphs
    for filename in files:
        print(f"\n{'='*60}")
        print(f"Call graph for: {filename}")
        print('='*60)
        
        try:
            _, events = read_all_events(filename)
            graph = build_call_graph(events, filter_obj)
            
            if args.format == 'tree':
                print_tree(graph, args.max_depth, args.min_calls)
            elif args.format == 'dot':
                output_file = args.output or f"{filename}.dot"
                export_dot(graph, output_file)
            else:
                print(f"Unknown format: {args.format}")
                
        except Exception as e:
            print(f"Error processing {filename}: {e}")

def compare_command(args):
    """Handle compare subcommand."""
    if len(args.files) != 2:
        print("Error: compare requires exactly 2 files")
        return
    
    file1, file2 = args.files
    print(f"Comparing {file1} vs {file2}")
    
    comparison = compare_traces(file1, file2, args.threshold)
    print_comparison(comparison)

def diff_command(args):
    """Handle diff subcommand."""
    if len(args.files) != 2:
        print("Error: diff requires exactly 2 files")
        return
    
    file1, file2 = args.files
    print(f"Diffing {file1} vs {file2}")
    
    diff_result = diff_traces(file1, file2)
    print_diff(diff_result)

def main():
    """Main CLI entry point."""
    parser = argparse.ArgumentParser(
        description='trace-scope: All-in-one tool for C++ tracing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f'''
Examples:
  # Add TRC_SCOPE() and TRC_ARG() to all functions:
  python trc.py instrument add myfile.cpp
  
  # Remove all trace calls:
  python trc.py instrument remove --all myfile.cpp
  
  # Display trace with filtering:
  python trc.py analyze trace.bin --function "main" --depth 3
  
  # Generate performance statistics:
  python trc.py stats trace.bin --sort total --csv results.csv
  
  # Generate call graph:
  python trc.py callgraph trace.bin --format tree --max-depth 5
  
  # Compare two traces:
  python trc.py compare trace1.bin trace2.bin --threshold 0.2
  
  # Diff two traces:
  python trc.py diff trace1.bin trace2.bin

Version: {__version__}
        '''
    )
    
    subparsers = parser.add_subparsers(dest='command', required=True)
    
    # instrument subcommand
    instrument_parser = subparsers.add_parser('instrument', help='Add/remove trace instrumentation')
    instrument_parser.add_argument('action', choices=['add', 'remove', 'remove-args', 'remove-msgs'],
                                   help='Action to perform')
    instrument_parser.add_argument('files', nargs='+', help='C++ source files to process')
    instrument_parser.add_argument('--no-args', action='store_true',
                                   help='For add: skip TRC_ARG() and add only TRC_SCOPE()')
    instrument_parser.add_argument('--all', action='store_true',
                                   help='For remove: remove all trace calls (SCOPE, ARG, MSG)')
    instrument_parser.add_argument('--dry-run', action='store_true',
                                   help='Preview changes without modifying files')
    instrument_parser.add_argument('--verbose', '-v', action='store_true',
                                   help='Show detailed processing information')
    
    # analyze subcommand
    analyze_parser = subparsers.add_parser('analyze', help='Display and analyze trace files')
    analyze_parser.add_argument('files', nargs='+', help='Trace files or directories to analyze')
    analyze_parser.add_argument('--function', action='append', help='Include only these functions (wildcards supported)')
    analyze_parser.add_argument('--file', action='append', help='Include only these files (wildcards supported)')
    analyze_parser.add_argument('--thread', type=int, action='append', help='Include only these thread IDs')
    analyze_parser.add_argument('--depth', type=int, default=-1, help='Maximum call depth (-1 for unlimited)')
    analyze_parser.add_argument('--pattern', default='*.trc', help='File pattern for directory search')
    analyze_parser.add_argument('--recursive', '-r', action='store_true', help='Search subdirectories')
    analyze_parser.add_argument('--sort', choices=['chronological', 'name', 'size'], default='chronological',
                               help='Sort method for directory search')
    analyze_parser.add_argument('--no-color', action='store_true', help='Disable color output')
    analyze_parser.add_argument('--no-timestamp', action='store_true', help='Hide timestamps')
    analyze_parser.add_argument('--no-timing', action='store_true', help='Hide timing information')
    
    # stats subcommand
    stats_parser = subparsers.add_parser('stats', help='Generate performance statistics')
    stats_parser.add_argument('files', nargs='+', help='Trace files or directories to analyze')
    stats_parser.add_argument('--function', action='append', help='Include only these functions')
    stats_parser.add_argument('--file', action='append', help='Include only these files')
    stats_parser.add_argument('--thread', type=int, action='append', help='Include only these thread IDs')
    stats_parser.add_argument('--depth', type=int, default=-1, help='Maximum call depth')
    stats_parser.add_argument('--pattern', default='*.trc', help='File pattern for directory search')
    stats_parser.add_argument('--recursive', '-r', action='store_true', help='Search subdirectories')
    stats_parser.add_argument('--sort', choices=['total', 'calls', 'avg', 'name'], default='total',
                              help='Sort statistics by this field')
    stats_parser.add_argument('--csv', help='Export statistics to CSV file')
    stats_parser.add_argument('--json', help='Export statistics to JSON file')
    
    # callgraph subcommand
    callgraph_parser = subparsers.add_parser('callgraph', help='Generate call graphs')
    callgraph_parser.add_argument('files', nargs='+', help='Trace files or directories to analyze')
    callgraph_parser.add_argument('--function', action='append', help='Include only these functions')
    callgraph_parser.add_argument('--file', action='append', help='Include only these files')
    callgraph_parser.add_argument('--thread', type=int, action='append', help='Include only these thread IDs')
    callgraph_parser.add_argument('--depth', type=int, default=-1, help='Maximum call depth')
    callgraph_parser.add_argument('--pattern', default='*.trc', help='File pattern for directory search')
    callgraph_parser.add_argument('--recursive', '-r', action='store_true', help='Search subdirectories')
    callgraph_parser.add_argument('--sort', choices=['chronological', 'name', 'size'], default='chronological',
                                  help='Sort method for directory search')
    callgraph_parser.add_argument('--format', choices=['tree', 'dot'], default='tree',
                                  help='Output format')
    callgraph_parser.add_argument('--output', '-o', help='Output file (for dot format)')
    callgraph_parser.add_argument('--max-depth', type=int, default=10, help='Maximum tree depth')
    callgraph_parser.add_argument('--min-calls', type=int, default=1, help='Minimum call count to include')
    
    # compare subcommand
    compare_parser = subparsers.add_parser('compare', help='Compare trace performance')
    compare_parser.add_argument('files', nargs=2, help='Two trace files to compare')
    compare_parser.add_argument('--threshold', type=float, default=0.1,
                                help='Performance difference threshold (0.1 = 10%)')
    
    # diff subcommand
    diff_parser = subparsers.add_parser('diff', help='Diff two trace files')
    diff_parser.add_argument('files', nargs=2, help='Two trace files to diff')
    
    args = parser.parse_args()
    
    # Route to appropriate command
    if args.command == 'instrument':
        instrument_command(args)
    elif args.command == 'analyze':
        analyze_command(args)
    elif args.command == 'stats':
        stats_command(args)
    elif args.command == 'callgraph':
        callgraph_command(args)
    elif args.command == 'compare':
        compare_command(args)
    elif args.command == 'diff':
        diff_command(args)

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
        sys.exit(1)
