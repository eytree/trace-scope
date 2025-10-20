#!/usr/bin/env python3
"""
Trace analysis tool for trace_scope binary dumps.

Multi-command tool for trace analysis:
  - display:   Pretty-print trace with filtering and colors
  - stats:     Performance metrics and statistics
  - callgraph: Call graph generation (coming soon)
  - compare:   Performance regression detection (coming soon)
  - diff:      Trace diff between runs (coming soon)
  - query:     Enhanced filtering/querying (coming soon)

Binary format (TRCLOG10):
  Version 1: type(1) + tid(4) + ts_ns(8) + depth(4) + dur_ns(8) +
             file_len(2) + file + func_len(2) + func + msg_len(2) + msg + line(4)
  
  Version 2: type(1) + tid(4) + color_offset(1) + ts_ns(8) + depth(4) + dur_ns(8) + memory_rss(8) +
             file_len(2) + file + func_len(2) + func + msg_len(2) + msg + line(4)

Features:
  - Thread-aware ANSI color output
  - Wildcard filtering by function name, file path, depth, and thread ID
  - Performance statistics with CSV/JSON export
  - Compatible with binary format versions 1 and 2
"""

import sys
import argparse

# Import common utilities
from trc_common import (
    EVENT_TYPE_ENTER, EVENT_TYPE_EXIT, EVENT_TYPE_MSG,
    COLORS, RESET,
    read_header, read_event, read_all_events,
    EventFilter,
    format_duration, format_memory, get_color,
    compute_stats, print_stats_table, export_csv, export_json
)


def print_event(event, use_color=False, show_timing=True, show_timestamp=False):
    """Format and print a single event."""
    color_start, color_end = get_color(event, use_color)
    
    # Optional timestamp
    ts_str = f"[{event['ts_ns']:16}] " if show_timestamp else ""
    
    # Thread ID (always show in hex)
    tid_str = f"[tid:0x{event['tid']:08x}] "
    
    # Indentation based on depth
    indent = '| ' * max(event['depth'], 0)
    
    # Format based on event type
    typ = event['type']
    if typ == EVENT_TYPE_ENTER:
        line = f"{ts_str}{tid_str}{color_start}{indent}-> {event['func']}{color_end} ({event['file']}:{event['line']})"
    elif typ == EVENT_TYPE_EXIT:
        timing = f"  [{format_duration(event['dur_ns'])}]" if show_timing else ""
        line = f"{ts_str}{tid_str}{color_start}{indent}<- {event['func']}{timing}{color_end} ({event['file']}:{event['line']})"
    else:  # Msg
        line = f"{ts_str}{tid_str}{color_start}{indent}- {event['msg']}{color_end} ({event['file']}:{event['line']})"
    
    print(line)


def cmd_display(args):
    """Display trace with pretty-printing and filtering."""
    # Build filter from args
    filt = EventFilter()
    filt.include_functions = args.filter_function
    filt.exclude_functions = args.exclude_function
    filt.include_files = args.filter_file
    filt.exclude_files = args.exclude_file
    filt.include_threads = args.filter_thread
    filt.exclude_threads = args.exclude_thread
    filt.max_depth = args.max_depth
    
    # Read and process
    version, events = read_all_events(args.file)
    print(f'# Binary format version: {version}', file=sys.stderr)
    
    # Filter and display
    displayed_count = 0
    for event in events:
        if filt.should_trace(event):
            print_event(event, args.color, not args.no_timing, args.timestamp)
            displayed_count += 1
    
    # Summary
    total_count = len(events)
    print(f'\n# Processed {total_count} events, displayed {displayed_count}', file=sys.stderr)
    if total_count != displayed_count:
        filtered = total_count - displayed_count
        pct = (filtered / total_count * 100) if total_count > 0 else 0
        print(f'# Filtered out {filtered} events ({pct:.1f}%)', file=sys.stderr)


def cmd_stats(args):
    """Display performance statistics."""
    # Build filter from args
    filt = EventFilter()
    filt.include_functions = args.filter_function
    filt.exclude_functions = args.exclude_function
    filt.include_files = args.filter_file
    filt.exclude_files = args.exclude_file
    filt.include_threads = args.filter_thread
    filt.exclude_threads = args.exclude_thread
    filt.max_depth = args.max_depth
    
    # Read events
    version, events = read_all_events(args.file)
    
    # Compute statistics
    global_stats, thread_stats = compute_stats(events, filt)
    
    # Export to files if requested
    if args.export_csv:
        export_csv(global_stats, thread_stats, args.export_csv)
    if args.export_json:
        export_json(global_stats, thread_stats, args.export_json)
    
    # Display table unless only exporting
    if not args.export_csv and not args.export_json:
        print_stats_table(global_stats, thread_stats, args.sort_by)


def cmd_callgraph(args):
    """Generate call graph (placeholder)."""
    print("Call graph generation - Coming soon!", file=sys.stderr)
    print("This will parse trace events and generate:", file=sys.stderr)
    print("  - Text tree (indented, ASCII art)", file=sys.stderr)
    print("  - GraphViz DOT format", file=sys.stderr)
    print("  - Call counts and timing annotations", file=sys.stderr)
    sys.exit(1)


def cmd_compare(args):
    """Performance regression detection (placeholder)."""
    print("Performance regression detection - Coming soon!", file=sys.stderr)
    print("This will compare two trace files and detect:", file=sys.stderr)
    print("  - Function-level duration changes", file=sys.stderr)
    print("  - Call count changes", file=sys.stderr)
    print("  - Memory usage changes", file=sys.stderr)
    sys.exit(1)


def cmd_diff(args):
    """Trace diff between runs (placeholder)."""
    print("Trace diff - Coming soon!", file=sys.stderr)
    print("This will compare execution paths between two traces:", file=sys.stderr)
    print("  - Functions called in A but not B", file=sys.stderr)
    print("  - Different call orders", file=sys.stderr)
    print("  - Unified diff style output", file=sys.stderr)
    sys.exit(1)


def cmd_query(args):
    """Enhanced filtering/querying (placeholder)."""
    print("Enhanced querying - Coming soon!", file=sys.stderr)
    print("This will provide SQL-like query syntax:", file=sys.stderr)
    print("  - Aggregate functions (COUNT, SUM, AVG, MIN, MAX)", file=sys.stderr)
    print("  - GROUP BY and HAVING clauses", file=sys.stderr)
    print("  - Time-series analysis", file=sys.stderr)
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Trace analysis tool for trace_scope binary dumps',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Display trace with colors
  trc_analyze.py display trace.bin --color
  
  # Filter to specific functions
  trc_analyze.py display trace.bin --filter-function "core_*"
  
  # Show performance statistics
  trc_analyze.py stats trace.bin
  
  # Export statistics to CSV
  trc_analyze.py stats trace.bin --export-csv stats.csv
  
  # Sort by call count
  trc_analyze.py stats trace.bin --sort-by calls
        '''
    )
    
    # Create subparsers for commands
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    subparsers.required = True
    
    # === DISPLAY COMMAND ===
    parser_display = subparsers.add_parser('display', help='Pretty-print trace with filtering')
    parser_display.add_argument('file', help='Binary trace file to process')
    
    # Color options
    parser_display.add_argument('--color', '--colour', action='store_true',
                                help='Enable ANSI color output (thread-aware)')
    parser_display.add_argument('--no-color', '--no-colour', dest='color', action='store_false',
                                help='Disable color output (default)')
    
    # Function filters
    parser_display.add_argument('--filter-function', '--include-function', action='append', default=[],
                                help='Include functions matching pattern (wildcard *)')
    parser_display.add_argument('--exclude-function', action='append', default=[],
                                help='Exclude functions matching pattern (wildcard *)')
    
    # File filters
    parser_display.add_argument('--filter-file', '--include-file', action='append', default=[],
                                help='Include files matching pattern (wildcard *)')
    parser_display.add_argument('--exclude-file', action='append', default=[],
                                help='Exclude files matching pattern (wildcard *)')
    
    # Thread filters
    parser_display.add_argument('--filter-thread', '--include-thread', action='append', default=[],
                                type=lambda x: int(x, 0),  # Supports 0x hex notation
                                help='Include specific thread IDs (hex: 0x1234 or decimal)')
    parser_display.add_argument('--exclude-thread', action='append', default=[],
                                type=lambda x: int(x, 0),
                                help='Exclude specific thread IDs')
    
    # Depth filter
    parser_display.add_argument('--max-depth', type=int, default=-1,
                                help='Maximum call depth to display (-1 = unlimited)')
    
    # Output options
    parser_display.add_argument('--no-timing', action='store_true',
                                help='Hide duration timing for Exit events')
    parser_display.add_argument('--timestamp', action='store_true',
                                help='Show absolute timestamps')
    
    parser_display.set_defaults(func=cmd_display)
    
    # === STATS COMMAND ===
    parser_stats = subparsers.add_parser('stats', help='Display performance statistics')
    parser_stats.add_argument('file', help='Binary trace file to process')
    
    # Function filters (same as display)
    parser_stats.add_argument('--filter-function', '--include-function', action='append', default=[],
                              help='Include functions matching pattern (wildcard *)')
    parser_stats.add_argument('--exclude-function', action='append', default=[],
                              help='Exclude functions matching pattern (wildcard *)')
    
    # File filters
    parser_stats.add_argument('--filter-file', '--include-file', action='append', default=[],
                              help='Include files matching pattern (wildcard *)')
    parser_stats.add_argument('--exclude-file', action='append', default=[],
                              help='Exclude files matching pattern (wildcard *)')
    
    # Thread filters
    parser_stats.add_argument('--filter-thread', '--include-thread', action='append', default=[],
                              type=lambda x: int(x, 0),
                              help='Include specific thread IDs (hex: 0x1234 or decimal)')
    parser_stats.add_argument('--exclude-thread', action='append', default=[],
                              type=lambda x: int(x, 0),
                              help='Exclude specific thread IDs')
    
    # Depth filter
    parser_stats.add_argument('--max-depth', type=int, default=-1,
                              help='Maximum call depth to include in stats (-1 = unlimited)')
    
    # Stats options
    parser_stats.add_argument('--sort-by', choices=['total', 'calls', 'avg', 'name'], default='total',
                              help='Sort statistics by: total time, call count, avg time, or function name')
    parser_stats.add_argument('--export-csv', metavar='FILE',
                              help='Export statistics to CSV file')
    parser_stats.add_argument('--export-json', metavar='FILE',
                              help='Export statistics to JSON file')
    
    parser_stats.set_defaults(func=cmd_stats)
    
    # === CALLGRAPH COMMAND (placeholder) ===
    parser_callgraph = subparsers.add_parser('callgraph', help='Generate call graph (coming soon)')
    parser_callgraph.add_argument('file', help='Binary trace file to process')
    parser_callgraph.set_defaults(func=cmd_callgraph)
    
    # === COMPARE COMMAND (placeholder) ===
    parser_compare = subparsers.add_parser('compare', help='Performance regression detection (coming soon)')
    parser_compare.add_argument('baseline', help='Baseline trace file')
    parser_compare.add_argument('current', help='Current trace file')
    parser_compare.set_defaults(func=cmd_compare)
    
    # === DIFF COMMAND (placeholder) ===
    parser_diff = subparsers.add_parser('diff', help='Trace diff between runs (coming soon)')
    parser_diff.add_argument('file_a', help='First trace file')
    parser_diff.add_argument('file_b', help='Second trace file')
    parser_diff.set_defaults(func=cmd_diff)
    
    # === QUERY COMMAND (placeholder) ===
    parser_query = subparsers.add_parser('query', help='Enhanced filtering/querying (coming soon)')
    parser_query.add_argument('file', help='Binary trace file to process')
    parser_query.set_defaults(func=cmd_query)
    
    # Parse and execute
    args = parser.parse_args()
    args.func(args)


if __name__ == '__main__':
    if len(sys.argv) == 1:
        print('usage: trc_analyze.py <command> [OPTIONS]', file=sys.stderr)
        print('       trc_analyze.py --help  for detailed usage', file=sys.stderr)
        print('', file=sys.stderr)
        print('Commands:', file=sys.stderr)
        print('  display    Pretty-print trace with filtering', file=sys.stderr)
        print('  stats      Display performance statistics', file=sys.stderr)
        print('  callgraph  Generate call graph (coming soon)', file=sys.stderr)
        print('  compare    Performance regression detection (coming soon)', file=sys.stderr)
        print('  diff       Trace diff between runs (coming soon)', file=sys.stderr)
        print('  query      Enhanced filtering/querying (coming soon)', file=sys.stderr)
        sys.exit(2)
    main()
