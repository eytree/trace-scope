#!/usr/bin/env python3
"""
Pretty-printer for trace_scope binary dump files with filtering and colors.

Binary format (TRCLOG10):
  Version 1: type(1) + tid(4) + ts_ns(8) + depth(4) + dur_ns(8) +
             file_len(2) + file + func_len(2) + func + msg_len(2) + msg + line(4)
  
  Version 2: type(1) + tid(4) + color_offset(1) + ts_ns(8) + depth(4) + dur_ns(8) +
             file_len(2) + file + func_len(2) + func + msg_len(2) + msg + line(4)

Features:
  - Thread-aware ANSI color output (matches C++ tracer)
  - Wildcard filtering by function name, file path, depth, and thread ID
  - Include/exclude filter lists (exclude wins)
  - Compatible with versions 1 and 2

Usage:
  trc_pretty.py trace.bin [OPTIONS]
  
Examples:
  trc_pretty.py trace.bin --color
  trc_pretty.py trace.bin --filter-function "core_*" --exclude-function "*_test"
  trc_pretty.py trace.bin --max-depth 10 --filter-thread 0x1234
"""

import struct
import sys
import argparse
import re


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


def read_event(f, version):
    """Read a single event based on format version."""
    (typ,) = struct.unpack('<B', readn(f, 1))
    (tid,) = struct.unpack('<I', readn(f, 4))
    
    if version >= 2:
        (color_offset,) = struct.unpack('<B', readn(f, 1))
    else:
        color_offset = 0  # Default for version 1
    
    (ts_ns,) = struct.unpack('<Q', readn(f, 8))
    (depth,) = struct.unpack('<I', readn(f, 4))
    (dur_ns,) = struct.unpack('<Q', readn(f, 8))
    
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
        'file': file,
        'func': func,
        'msg': msg,
        'line': line
    }


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


def get_color(event, use_color):
    """Get ANSI color code for event (thread-aware)."""
    if not use_color:
        return '', ''
    
    color_idx = (event['depth'] + event['color_offset']) % 8
    return COLORS[color_idx], RESET


def print_event(event, use_color=False, show_timing=True, show_timestamp=False):
    """Format and print a single event."""
    color_start, color_end = get_color(event, use_color)
    
    # Thread ID
    tid_str = f"({event['tid']:08x}) "
    
    # Timestamp (optional)
    ts_str = f"[{event['ts_ns']}] " if show_timestamp else ""
    
    # Indent markers
    indent = '| ' * max(event['depth'], 0)
    
    # Format based on event type
    typ = event['type']
    if typ == 0:  # Enter
        line = f"{ts_str}{tid_str}{color_start}{indent}-> {event['func']}{color_end} ({event['file']}:{event['line']})"
    elif typ == 1:  # Exit
        timing = f"  [{format_duration(event['dur_ns'])}]" if show_timing else ""
        line = f"{ts_str}{tid_str}{color_start}{indent}<- {event['func']}{timing}{color_end} ({event['file']}:{event['line']})"
    else:  # Msg
        line = f"{ts_str}{tid_str}{color_start}{indent}- {event['msg']}{color_end} ({event['file']}:{event['line']})"
    
    print(line)


def process_trace(path, filt, use_color=False, show_timing=True, show_timestamp=False):
    """Parse and display a trace binary file with filters."""
    with open(path, 'rb') as f:
        # Read header
        magic = readn(f, 8)
        if magic != b'TRCLOG10':
            raise SystemExit(f'Error: Bad magic (expected TRCLOG10, got {magic})')
        
        version, padding = struct.unpack('<II', readn(f, 8))
        if version < 1 or version > 2:
            raise SystemExit(f'Error: Unsupported version {version} (expected 1 or 2)')
        
        # Print version info
        print(f'# Binary format version: {version}', file=sys.stderr)
        
        # Read and filter events
        total_count = 0
        displayed_count = 0
        
        try:
            while True:
                event = read_event(f, version)
                total_count += 1
                
                if filt.should_trace(event):
                    print_event(event, use_color, show_timing, show_timestamp)
                    displayed_count += 1
        except EOFError:
            pass
    
    # Summary
    print(f'\n# Processed {total_count} events, displayed {displayed_count}', file=sys.stderr)
    if total_count != displayed_count:
        filtered = total_count - displayed_count
        pct = (filtered / total_count * 100) if total_count > 0 else 0
        print(f'# Filtered out {filtered} events ({pct:.1f}%)', file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description='Pretty-print trace_scope binary dumps with filtering and colors',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Basic usage
  trc_pretty.py trace.bin
  
  # With colors
  trc_pretty.py trace.bin --color
  
  # Filter to specific functions
  trc_pretty.py trace.bin --filter-function "core_*"
  
  # Exclude test functions
  trc_pretty.py trace.bin --exclude-function "*_test"
  
  # Multiple filters
  trc_pretty.py trace.bin --color --max-depth 10 \\
      --filter-function "my_namespace::*" \\
      --exclude-function "debug_*"
  
  # Filter by thread ID
  trc_pretty.py trace.bin --filter-thread 0x12345678
        '''
    )
    
    parser.add_argument('file', help='Binary trace file to process')
    
    # Color options
    parser.add_argument('--color', '--colour', action='store_true',
                        help='Enable ANSI color output (thread-aware)')
    parser.add_argument('--no-color', '--no-colour', dest='color', action='store_false',
                        help='Disable color output (default)')
    
    # Function filters
    parser.add_argument('--filter-function', '--include-function', action='append', default=[],
                        help='Include functions matching pattern (wildcard *)')
    parser.add_argument('--exclude-function', action='append', default=[],
                        help='Exclude functions matching pattern (wildcard *)')
    
    # File filters
    parser.add_argument('--filter-file', '--include-file', action='append', default=[],
                        help='Include files matching pattern (wildcard *)')
    parser.add_argument('--exclude-file', action='append', default=[],
                        help='Exclude files matching pattern (wildcard *)')
    
    # Thread filters
    parser.add_argument('--filter-thread', '--include-thread', action='append', default=[],
                        type=lambda x: int(x, 0),  # Supports 0x hex notation
                        help='Include specific thread IDs (hex: 0x1234 or decimal)')
    parser.add_argument('--exclude-thread', action='append', default=[],
                        type=lambda x: int(x, 0),
                        help='Exclude specific thread IDs')
    
    # Depth filter
    parser.add_argument('--max-depth', type=int, default=-1,
                        help='Maximum call depth to display (-1 = unlimited)')
    
    # Output options
    parser.add_argument('--no-timing', action='store_true',
                        help='Hide duration timing for Exit events')
    parser.add_argument('--timestamp', action='store_true',
                        help='Show absolute timestamps')
    
    args = parser.parse_args()
    
    # Build filter from args
    filt = EventFilter()
    filt.include_functions = args.filter_function
    filt.exclude_functions = args.exclude_function
    filt.include_files = args.filter_file
    filt.exclude_files = args.exclude_file
    filt.include_threads = args.filter_thread
    filt.exclude_threads = args.exclude_thread
    filt.max_depth = args.max_depth
    
    # Process file
    process_trace(args.file, filt, args.color, not args.no_timing, args.timestamp)


if __name__ == '__main__':
    if len(sys.argv) == 1:
        print('usage: trc_pretty.py trace.bin [OPTIONS]', file=sys.stderr)
        print('       trc_pretty.py --help  for detailed usage', file=sys.stderr)
        sys.exit(2)
    main()
