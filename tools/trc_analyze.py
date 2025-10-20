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
import os
import glob
from pathlib import Path

# Import common utilities
from trc_common import (
    __version__,
    EVENT_TYPE_ENTER, EVENT_TYPE_EXIT, EVENT_TYPE_MSG,
    COLORS, RESET,
    read_header, read_event, read_all_events,
    EventFilter,
    format_duration, format_memory, get_color,
    compute_stats, print_stats_table, export_csv, export_json
)


def process_directory(path, pattern='*.trc', recursive=False, sort_by='chronological'):
    """
    Find and sort trace files in a directory.
    
    Args:
        path: Directory path to search
        pattern: File pattern to match (default: *.trc)
        recursive: Search subdirectories recursively (default: False)
        sort_by: Sort order - 'chronological' (mtime), 'name', or 'size' (default: chronological)
    
    Returns:
        List of file paths sorted according to sort_by parameter
    """
    dir_path = Path(path)
    
    if not dir_path.exists():
        print(f"Error: Directory not found: {path}", file=sys.stderr)
        return []
    
    if not dir_path.is_dir():
        print(f"Error: Not a directory: {path}", file=sys.stderr)
        return []
    
    # Find files
    files = []
    if recursive:
        files = list(dir_path.rglob(pattern))
    else:
        files = list(dir_path.glob(pattern))
    
    if not files:
        print(f"No {pattern} files found in {path}", file=sys.stderr)
        return []
    
    # Sort files
    if sort_by == 'name':
        files.sort(key=lambda f: f.name)
    elif sort_by == 'size':
        files.sort(key=lambda f: f.stat().st_size)
    else:  # chronological (mtime)
        files.sort(key=lambda f: f.stat().st_mtime)
    
    return [str(f) for f in files]


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
    
    # Determine if input is file or directory
    input_path = Path(args.input)
    files = []
    
    if input_path.is_dir():
        # Process directory
        files = process_directory(args.input, pattern='*.trc', 
                                   recursive=args.recursive,
                                   sort_by=args.sort_files)
        if not files:
            return
        print(f'# Processing {len(files)} files from directory: {args.input}', file=sys.stderr)
    elif input_path.is_file():
        files = [args.input]
    else:
        print(f'Error: File or directory not found: {args.input}', file=sys.stderr)
        return
    
    # Process each file
    total_events_all = 0
    total_displayed_all = 0
    
    for file_path in files:
        if len(files) > 1:
            print(f'\n# ===== {file_path} =====', file=sys.stderr)
        
        # Read and process
        version, events = read_all_events(file_path)
        if len(files) == 1:
            print(f'# Binary format version: {version}', file=sys.stderr)
        
        # Filter and display
        displayed_count = 0
        for event in events:
            if filt.should_trace(event):
                print_event(event, args.color, not args.no_timing, args.timestamp)
                displayed_count += 1
        
        total_events_all += len(events)
        total_displayed_all += displayed_count
        
        # Per-file summary (if multiple files)
        if len(files) > 1:
            filtered = len(events) - displayed_count
            pct = (filtered / len(events) * 100) if len(events) > 0 else 0
            print(f'# File: {displayed_count}/{len(events)} events displayed ({filtered} filtered, {pct:.1f}%)', 
                  file=sys.stderr)
    
    # Overall summary
    print(f'\n# Total: Processed {total_events_all} events, displayed {total_displayed_all}', file=sys.stderr)
    if total_events_all != total_displayed_all:
        filtered = total_events_all - total_displayed_all
        pct = (filtered / total_events_all * 100) if total_events_all > 0 else 0
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
    
    # Determine if input is file or directory
    input_path = Path(args.input)
    files = []
    
    if input_path.is_dir():
        # Process directory
        files = process_directory(args.input, pattern='*.trc', 
                                   recursive=args.recursive,
                                   sort_by=args.sort_files)
        if not files:
            return
        print(f'# Processing {len(files)} files from directory: {args.input}', file=sys.stderr)
    elif input_path.is_file():
        files = [args.input]
    else:
        print(f'Error: File or directory not found: {args.input}', file=sys.stderr)
        return
    
    # Aggregate events from all files
    all_events = []
    for file_path in files:
        version, events = read_all_events(file_path)
        all_events.extend(events)
    
    if len(files) > 1:
        print(f'# Aggregated {len(all_events)} events from {len(files)} files', file=sys.stderr)
    
    # Compute statistics on aggregated events
    global_stats, thread_stats = compute_stats(all_events, filt)
    
    # Export to files if requested
    if args.export_csv:
        export_csv(global_stats, thread_stats, args.export_csv)
    if args.export_json:
        export_json(global_stats, thread_stats, args.export_json)
    
    # Display table unless only exporting
    if not args.export_csv and not args.export_json:
        print_stats_table(global_stats, thread_stats, args.sort_by)


def cmd_callgraph(args):
    """Generate call graph."""
    from trc_callgraph import build_call_graph, format_text_tree, format_graphviz_dot, print_call_graph_summary
    
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
    version, events = read_all_events(args.input)
    
    # Build call graph
    graph = build_call_graph(events, filt)
    
    # Print summary
    if not args.quiet:
        print_call_graph_summary(graph)
    
    # Generate output based on format
    if args.format == 'tree':
        output = format_text_tree(graph, 
                                  show_counts=args.show_counts,
                                  show_durations=args.show_durations,
                                  max_depth=args.tree_max_depth)
        
        if args.output:
            with open(args.output, 'w', encoding='utf-8') as f:
                f.write(output)
            print(f"Call graph saved to {args.output}")
        else:
            print(output)
    
    elif args.format == 'dot':
        output = format_graphviz_dot(graph,
                                     show_counts=args.show_counts,
                                     show_durations=args.show_durations,
                                     colorize=args.colorize)
        
        if args.output:
            with open(args.output, 'w', encoding='utf-8') as f:
                f.write(output)
            print(f"GraphViz DOT file saved to {args.output}")
            print(f"  Generate image: dot -Tpng {args.output} -o callgraph.png")
        else:
            print(output)


def cmd_compare(args):
    """Performance regression detection."""
    from trc_compare import compare_traces, print_comparison_report, export_comparison_csv, export_comparison_json
    
    # Build filter from args
    filt = EventFilter()
    filt.include_functions = args.filter_function
    filt.exclude_functions = args.exclude_function
    filt.include_files = args.filter_file
    filt.exclude_files = args.exclude_file
    filt.include_threads = args.filter_thread
    filt.exclude_threads = args.exclude_thread
    filt.max_depth = args.max_depth
    
    # Compare traces
    comparison = compare_traces(args.baseline, args.current, filt, args.threshold)
    
    # Export if requested
    if args.export_csv:
        export_comparison_csv(comparison, args.export_csv)
    if args.export_json:
        export_comparison_json(comparison, args.export_json)
    
    # Print report unless only exporting
    if not args.export_csv and not args.export_json:
        print_comparison_report(comparison, args.show_all)
    
    # Exit with error code if regressions found
    if args.fail_on_regression and comparison['regressions']:
        print(f"\n✗ FAIL: {len(comparison['regressions'])} regression(s) detected", file=sys.stderr)
        sys.exit(1)


def cmd_diff(args):
    """Trace diff between runs."""
    from trc_diff import ExecutionPath, compute_diff, print_diff_report, export_diff_json
    
    # Read events from both files
    _, events_a = read_all_events(args.file_a)
    _, events_b = read_all_events(args.file_b)
    
    # Build execution paths
    path_a = ExecutionPath(events_a)
    path_b = ExecutionPath(events_b)
    
    # Compute diff
    diff = compute_diff(path_a, path_b)
    
    # Export if requested
    if args.export_json:
        export_diff_json(diff, args.export_json, args.file_a, args.file_b)
    
    # Print report unless only exporting
    if not args.export_json:
        print_diff_report(diff, args.file_a, args.file_b)


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
        description=f'Trace analysis tool for trace_scope binary dumps (v{__version__})',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Display trace with colors
  trc_analyze.py display trace.trc --color
  
  # Process directory of traces
  trc_analyze.py display logs/ --recursive
  
  # Filter to specific functions
  trc_analyze.py display trace.trc --filter-function "core_*"
  
  # Show performance statistics
  trc_analyze.py stats trace.trc
  
  # Export statistics to CSV
  trc_analyze.py stats trace.trc --export-csv stats.csv
  
  # Sort by call count
  trc_analyze.py stats trace.trc --sort-by calls
        '''
    )
    
    # Add version flag
    parser.add_argument('--version', action='version', version=f'trace-scope v{__version__}')
    
    # Create subparsers for commands
    subparsers = parser.add_subparsers(dest='command', help='Command to execute')
    subparsers.required = True
    
    # === DISPLAY COMMAND ===
    parser_display = subparsers.add_parser('display', help='Pretty-print trace with filtering')
    parser_display.add_argument('input', help='Binary trace file or directory to process')
    
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
    
    # Directory processing options
    parser_display.add_argument('--recursive', '-r', action='store_true',
                                help='Recursively search subdirectories for trace files')
    parser_display.add_argument('--sort-files', choices=['chronological', 'name', 'size'], 
                                default='chronological',
                                help='Sort order for multiple files (default: chronological by mtime)')
    
    parser_display.set_defaults(func=cmd_display)
    
    # === STATS COMMAND ===
    parser_stats = subparsers.add_parser('stats', help='Display performance statistics')
    parser_stats.add_argument('input', help='Binary trace file or directory to process')
    
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
    
    # Directory processing options
    parser_stats.add_argument('--recursive', '-r', action='store_true',
                              help='Recursively search subdirectories for trace files')
    parser_stats.add_argument('--sort-files', choices=['chronological', 'name', 'size'], 
                              default='chronological',
                              help='Sort order for multiple files (default: chronological by mtime)')
    
    parser_stats.set_defaults(func=cmd_stats)
    
    # === CALLGRAPH COMMAND ===
    parser_callgraph = subparsers.add_parser('callgraph', help='Generate call graph')
    parser_callgraph.add_argument('input', help='Binary trace file to process')
    
    # Output format
    parser_callgraph.add_argument('--format', '-f', choices=['tree', 'dot'], default='tree',
                                  help='Output format: tree (text) or dot (GraphViz)')
    parser_callgraph.add_argument('--output', '-o', metavar='FILE',
                                  help='Output file (default: stdout)')
    
    # Display options
    parser_callgraph.add_argument('--show-counts', action='store_true', default=True,
                                  help='Show call counts (default: enabled)')
    parser_callgraph.add_argument('--no-counts', dest='show_counts', action='store_false',
                                  help='Hide call counts')
    parser_callgraph.add_argument('--show-durations', action='store_true', default=True,
                                  help='Show timing information (default: enabled)')
    parser_callgraph.add_argument('--no-durations', dest='show_durations', action='store_false',
                                  help='Hide timing information')
    parser_callgraph.add_argument('--tree-max-depth', type=int, default=-1,
                                  help='Maximum tree depth to display (-1 = unlimited)')
    parser_callgraph.add_argument('--colorize', action='store_true', default=True,
                                  help='Colorize DOT nodes by duration (default: enabled)')
    parser_callgraph.add_argument('--no-color', dest='colorize', action='store_false',
                                  help='Disable DOT node coloring')
    parser_callgraph.add_argument('--quiet', '-q', action='store_true',
                                  help='Suppress summary output')
    
    # Function filters (same as display/stats)
    parser_callgraph.add_argument('--filter-function', '--include-function', action='append', default=[],
                                  help='Include functions matching pattern (wildcard *)')
    parser_callgraph.add_argument('--exclude-function', action='append', default=[],
                                  help='Exclude functions matching pattern (wildcard *)')
    
    # File filters
    parser_callgraph.add_argument('--filter-file', '--include-file', action='append', default=[],
                                  help='Include files matching pattern (wildcard *)')
    parser_callgraph.add_argument('--exclude-file', action='append', default=[],
                                  help='Exclude files matching pattern (wildcard *)')
    
    # Thread filters
    parser_callgraph.add_argument('--filter-thread', '--include-thread', action='append', default=[],
                                  type=lambda x: int(x, 0),
                                  help='Include specific thread IDs (hex: 0x1234 or decimal)')
    parser_callgraph.add_argument('--exclude-thread', action='append', default=[],
                                  type=lambda x: int(x, 0),
                                  help='Exclude specific thread IDs')
    
    # Depth filter
    parser_callgraph.add_argument('--max-depth', type=int, default=-1,
                                  help='Maximum call depth to include (-1 = unlimited)')
    
    parser_callgraph.set_defaults(func=cmd_callgraph)
    
    # === COMPARE COMMAND ===
    parser_compare = subparsers.add_parser('compare', help='Performance regression detection')
    parser_compare.add_argument('baseline', help='Baseline trace file')
    parser_compare.add_argument('current', help='Current trace file')
    
    # Comparison options
    parser_compare.add_argument('--threshold', '-t', type=float, default=5.0,
                                help='Minimum percentage change to report (default: 5.0%%)')
    parser_compare.add_argument('--show-all', action='store_true',
                                help='Show all regressions/improvements (not just top N)')
    parser_compare.add_argument('--fail-on-regression', action='store_true',
                                help='Exit with error code if regressions detected (for CI/CD)')
    
    # Export options
    parser_compare.add_argument('--export-csv', metavar='FILE',
                                help='Export comparison to CSV file')
    parser_compare.add_argument('--export-json', metavar='FILE',
                                help='Export comparison to JSON file')
    
    # Function filters
    parser_compare.add_argument('--filter-function', '--include-function', action='append', default=[],
                                help='Include functions matching pattern (wildcard *)')
    parser_compare.add_argument('--exclude-function', action='append', default=[],
                                help='Exclude functions matching pattern (wildcard *)')
    
    # File filters
    parser_compare.add_argument('--filter-file', '--include-file', action='append', default=[],
                                help='Include files matching pattern (wildcard *)')
    parser_compare.add_argument('--exclude-file', action='append', default=[],
                                help='Exclude files matching pattern (wildcard *)')
    
    # Thread filters
    parser_compare.add_argument('--filter-thread', '--include-thread', action='append', default=[],
                                type=lambda x: int(x, 0),
                                help='Include specific thread IDs (hex: 0x1234 or decimal)')
    parser_compare.add_argument('--exclude-thread', action='append', default=[],
                                type=lambda x: int(x, 0),
                                help='Exclude specific thread IDs')
    
    # Depth filter
    parser_compare.add_argument('--max-depth', type=int, default=-1,
                                help='Maximum call depth to include (-1 = unlimited)')
    
    parser_compare.set_defaults(func=cmd_compare)
    
    # === DIFF COMMAND ===
    parser_diff = subparsers.add_parser('diff', help='Trace diff between runs')
    parser_diff.add_argument('file_a', help='First trace file')
    parser_diff.add_argument('file_b', help='Second trace file')
    
    # Export options
    parser_diff.add_argument('--export-json', metavar='FILE',
                            help='Export diff results to JSON file')
    
    parser_diff.set_defaults(func=cmd_diff)
    
    # === QUERY COMMAND (placeholder) ===
    parser_query = subparsers.add_parser('query', help='Enhanced filtering/querying (coming soon)')
    parser_query.add_argument('input', help='Binary trace file to process')
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
