#!/usr/bin/env python3
"""
Common utilities for trace_scope binary format parsing and analysis.

Shared functionality:
- Binary format reading (version 1 & 2 support)
- Event filtering (EventFilter class)
- Statistics computation
- Color handling (ANSI codes)
- Format helpers (duration, memory)
"""

import struct
import re


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

