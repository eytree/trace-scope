#!/usr/bin/env python3
"""
Trace diff - Compare execution paths between two trace runs.

Features:
- Detect functions called in A but not B
- Detect functions called in B but not A
- Detect different call orders
- Detect different call depths
- Unified diff style output
"""

from trc_common import read_all_events, EVENT_TYPE_ENTER, EVENT_TYPE_EXIT


class ExecutionPath:
    """Represents an execution path from a trace."""
    
    def __init__(self, events):
        self.events = events
        self.call_sequence = []  # [(func, depth), ...]
        self.function_set = set()
        self._build_sequence()
    
    def _build_sequence(self):
        """Build call sequence from events."""
        for event in self.events:
            if event['type'] == EVENT_TYPE_ENTER:
                func = event['func']
                depth = event['depth']
                self.call_sequence.append((func, depth))
                self.function_set.add(func)


def compute_diff(path_a, path_b):
    """
    Compute differences between two execution paths.
    
    Args:
        path_a: ExecutionPath for first trace
        path_b: ExecutionPath for second trace
    
    Returns:
        dict with keys: only_in_a, only_in_b, common, sequence_diff
    """
    only_in_a = sorted(path_a.function_set - path_b.function_set)
    only_in_b = sorted(path_b.function_set - path_a.function_set)
    common = sorted(path_a.function_set & path_b.function_set)
    
    # Find sequence differences using simple comparison
    # (More sophisticated: use LCS algorithm)
    sequence_diff = []
    max_len = min(len(path_a.call_sequence), len(path_b.call_sequence))
    
    for i in range(max_len):
        func_a, depth_a = path_a.call_sequence[i]
        func_b, depth_b = path_b.call_sequence[i]
        
        if func_a != func_b or depth_a != depth_b:
            sequence_diff.append({
                'index': i,
                'a': (func_a, depth_a),
                'b': (func_b, depth_b)
            })
    
    # Handle length differences
    if len(path_a.call_sequence) > len(path_b.call_sequence):
        for i in range(max_len, len(path_a.call_sequence)):
            func, depth = path_a.call_sequence[i]
            sequence_diff.append({
                'index': i,
                'a': (func, depth),
                'b': None
            })
    elif len(path_b.call_sequence) > len(path_a.call_sequence):
        for i in range(max_len, len(path_b.call_sequence)):
            func, depth = path_b.call_sequence[i]
            sequence_diff.append({
                'index': i,
                'a': None,
                'b': (func, depth)
            })
    
    return {
        'only_in_a': only_in_a,
        'only_in_b': only_in_b,
        'common': common,
        'sequence_diff': sequence_diff[:50]  # Limit to first 50 differences
    }


def print_diff_report(diff, file_a_name, file_b_name):
    """Print diff report in unified diff style."""
    print("\n" + "=" * 100)
    print(f" Trace Diff: {file_a_name} vs {file_b_name}")
    print("=" * 100)
    
    # Summary
    total_only_a = len(diff['only_in_a'])
    total_only_b = len(diff['only_in_b'])
    total_common = len(diff['common'])
    total_seq_diff = len(diff['sequence_diff'])
    
    print(f"\nSummary:")
    print(f"  Functions only in A: {total_only_a}")
    print(f"  Functions only in B: {total_only_b}")
    print(f"  Common functions: {total_common}")
    print(f"  Sequence differences: {total_seq_diff}")
    
    # Functions only in A (removed)
    if diff['only_in_a']:
        print(f"\n- FUNCTIONS ONLY IN A ({total_only_a}):")
        print("-" * 100)
        for func in diff['only_in_a'][:20]:
            print(f"  - {func}")
        if total_only_a > 20:
            print(f"  ... and {total_only_a - 20} more")
    
    # Functions only in B (added)
    if diff['only_in_b']:
        print(f"\n+ FUNCTIONS ONLY IN B ({total_only_b}):")
        print("-" * 100)
        for func in diff['only_in_b'][:20]:
            print(f"  + {func}")
        if total_only_b > 20:
            print(f"  ... and {total_only_b - 20} more")
    
    # Sequence differences
    if diff['sequence_diff']:
        print(f"\n~ SEQUENCE DIFFERENCES (showing first {min(20, total_seq_diff)}):")
        print("-" * 100)
        for item in diff['sequence_diff'][:20]:
            idx = item['index']
            a_info = item['a']
            b_info = item['b']
            
            if a_info is None:
                # Only in B
                func_b, depth_b = b_info
                indent_b = "  " * depth_b
                print(f"  [{idx:4}]     (none)                           vs  + {indent_b}{func_b}")
            elif b_info is None:
                # Only in A
                func_a, depth_a = a_info
                indent_a = "  " * depth_a
                print(f"  [{idx:4}]   - {indent_a}{func_a}               vs    (none)")
            else:
                # Different
                func_a, depth_a = a_info
                func_b, depth_b = b_info
                indent_a = "  " * depth_a
                indent_b = "  " * depth_b
                print(f"  [{idx:4}]   ~ {indent_a}{func_a}               vs  ~ {indent_b}{func_b}")
        
        if total_seq_diff > 20:
            print(f"  ... and {total_seq_diff - 20} more differences")
    
    # Overall verdict
    print("\n" + "=" * 100)
    if not diff['only_in_a'] and not diff['only_in_b'] and not diff['sequence_diff']:
        print("✓ IDENTICAL EXECUTION PATHS")
    else:
        print("✗ EXECUTION PATHS DIFFER")
    print("=" * 100 + "\n")


def export_diff_json(diff, filename, file_a_name, file_b_name):
    """Export diff results to JSON file."""
    import json
    
    data = {
        'file_a': file_a_name,
        'file_b': file_b_name,
        'only_in_a': diff['only_in_a'],
        'only_in_b': diff['only_in_b'],
        'common': diff['common'],
        'sequence_differences': [{
            'index': item['index'],
            'a': {'function': item['a'][0], 'depth': item['a'][1]} if item['a'] else None,
            'b': {'function': item['b'][0], 'depth': item['b'][1]} if item['b'] else None
        } for item in diff['sequence_diff']]
    }
    
    with open(filename, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    
    print(f"Diff report exported to {filename}")

