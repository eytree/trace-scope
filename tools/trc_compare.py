#!/usr/bin/env python3
"""
Performance regression detection by comparing two trace files.

Features:
- Compare baseline vs current trace
- Detect duration regressions/improvements
- Detect call count changes
- Detect memory usage changes
- Identify new/removed functions
- Export to CSV/JSON
"""

from trc_common import read_all_events, compute_stats, EventFilter, format_duration, format_memory


class Regression:
    """Represents a performance regression or improvement."""
    
    def __init__(self, func_name, metric_name, baseline_value, current_value):
        self.func_name = func_name
        self.metric_name = metric_name
        self.baseline_value = baseline_value
        self.current_value = current_value
        self.delta = current_value - baseline_value
        self.percent_change = ((current_value - baseline_value) / baseline_value * 100) if baseline_value > 0 else float('inf')
        self.is_regression = current_value > baseline_value if metric_name in ['avg_ns', 'total_ns', 'memory_delta'] else False
    
    def __repr__(self):
        sign = "+" if self.delta >= 0 else ""
        return f"{self.func_name}: {self.metric_name} {sign}{self.percent_change:.1f}%"


def compare_traces(baseline_file, current_file, filter_obj=None, threshold=5.0):
    """
    Compare two trace files and detect regressions.
    
    Args:
        baseline_file: Path to baseline trace file
        current_file: Path to current trace file
        filter_obj: Optional EventFilter to apply
        threshold: Minimum percentage change to report (default: 5%)
    
    Returns:
        dict with keys: regressions, improvements, new_functions, removed_functions
    """
    # Read and compute stats for both files
    _, baseline_events = read_all_events(baseline_file)
    _, current_events = read_all_events(current_file)
    
    baseline_stats, _ = compute_stats(baseline_events, filter_obj or EventFilter())
    current_stats, _ = compute_stats(current_events, filter_obj or EventFilter())
    
    regressions = []
    improvements = []
    new_functions = []
    removed_functions = []
    
    # Find all functions
    all_functions = set(baseline_stats.keys()) | set(current_stats.keys())
    
    for func in all_functions:
        if func not in baseline_stats:
            # New function
            new_functions.append(func)
            continue
        
        if func not in current_stats:
            # Removed function
            removed_functions.append(func)
            continue
        
        baseline = baseline_stats[func]
        current = current_stats[func]
        
        # Compare average duration
        if baseline['avg_ns'] > 0:
            r = Regression(func, 'avg_ns', baseline['avg_ns'], current['avg_ns'])
            if abs(r.percent_change) >= threshold:
                if r.is_regression:
                    regressions.append(r)
                else:
                    improvements.append(r)
        
        # Compare total duration
        if baseline['total_ns'] > 0:
            r = Regression(func, 'total_ns', baseline['total_ns'], current['total_ns'])
            if abs(r.percent_change) >= threshold:
                if r.is_regression:
                    regressions.append(r)
                else:
                    improvements.append(r)
        
        # Compare call count
        r = Regression(func, 'calls', baseline['calls'], current['calls'])
        if abs(r.percent_change) >= threshold:
            regressions.append(r)  # Call count changes are informational, not good/bad
        
        # Compare memory
        if baseline['memory_delta'] > 0 or current['memory_delta'] > 0:
            r = Regression(func, 'memory_delta', baseline['memory_delta'], current['memory_delta'])
            if abs(r.percent_change) >= threshold:
                if r.is_regression:
                    regressions.append(r)
                else:
                    improvements.append(r)
    
    return {
        'regressions': sorted(regressions, key=lambda x: abs(x.percent_change), reverse=True),
        'improvements': sorted(improvements, key=lambda x: abs(x.percent_change), reverse=True),
        'new_functions': sorted(new_functions),
        'removed_functions': sorted(removed_functions)
    }


def print_comparison_report(comparison, show_all=False):
    """Print comparison report to console."""
    print("\n" + "=" * 100)
    print(" Performance Comparison Report")
    print("=" * 100)
    
    # Regressions
    regressions = comparison['regressions']
    if regressions:
        print(f"\n⚠ REGRESSIONS DETECTED ({len(regressions)}):")
        print("-" * 100)
        print(f"{'Function':<40} {'Metric':<15} {'Baseline':>15} {'Current':>15} {'Change':>15}")
        print("-" * 100)
        
        for r in regressions[:20 if not show_all else None]:  # Show top 20 by default
            baseline_str = format_duration(r.baseline_value) if 'ns' in r.metric_name else \
                          format_memory(r.baseline_value) if 'memory' in r.metric_name else \
                          str(int(r.baseline_value))
            current_str = format_duration(r.current_value) if 'ns' in r.metric_name else \
                         format_memory(r.current_value) if 'memory' in r.metric_name else \
                         str(int(r.current_value))
            
            print(f"{r.func_name:<40} {r.metric_name:<15} {baseline_str:>15} {current_str:>15} {r.percent_change:>+14.1f}%")
        
        if len(regressions) > 20 and not show_all:
            print(f"\n  ... and {len(regressions) - 20} more (use --show-all to see all)")
    else:
        print("\n✓ NO REGRESSIONS DETECTED")
    
    # Improvements
    improvements = comparison['improvements']
    if improvements:
        print(f"\n✓ IMPROVEMENTS ({len(improvements)}):")
        print("-" * 100)
        print(f"{'Function':<40} {'Metric':<15} {'Baseline':>15} {'Current':>15} {'Change':>15}")
        print("-" * 100)
        
        for r in improvements[:10 if not show_all else None]:  # Show top 10 by default
            baseline_str = format_duration(r.baseline_value) if 'ns' in r.metric_name else \
                          format_memory(r.baseline_value) if 'memory' in r.metric_name else \
                          str(int(r.baseline_value))
            current_str = format_duration(r.current_value) if 'ns' in r.metric_name else \
                         format_memory(r.current_value) if 'memory' in r.metric_name else \
                         str(int(r.current_value))
            
            print(f"{r.func_name:<40} {r.metric_name:<15} {baseline_str:>15} {current_str:>15} {r.percent_change:>+14.1f}%")
        
        if len(improvements) > 10 and not show_all:
            print(f"\n  ... and {len(improvements) - 10} more (use --show-all to see all)")
    
    # New functions
    new_funcs = comparison['new_functions']
    if new_funcs:
        print(f"\n+ NEW FUNCTIONS ({len(new_funcs)}):")
        for func in new_funcs[:10 if not show_all else None]:
            print(f"  + {func}")
        if len(new_funcs) > 10 and not show_all:
            print(f"  ... and {len(new_funcs) - 10} more")
    
    # Removed functions
    removed_funcs = comparison['removed_functions']
    if removed_funcs:
        print(f"\n- REMOVED FUNCTIONS ({len(removed_funcs)}):")
        for func in removed_funcs[:10 if not show_all else None]:
            print(f"  - {func}")
        if len(removed_funcs) > 10 and not show_all:
            print(f"  ... and {len(removed_funcs) - 10} more")
    
    print("\n" + "=" * 100 + "\n")


def export_comparison_csv(comparison, filename):
    """Export comparison results to CSV file."""
    import csv
    
    with open(filename, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        
        # Header
        writer.writerow(['Type', 'Function', 'Metric', 'Baseline', 'Current', 'Delta', 'Percent_Change'])
        
        # Regressions
        for r in comparison['regressions']:
            writer.writerow(['REGRESSION', r.func_name, r.metric_name, 
                           r.baseline_value, r.current_value, r.delta, f"{r.percent_change:.2f}"])
        
        # Improvements
        for r in comparison['improvements']:
            writer.writerow(['IMPROVEMENT', r.func_name, r.metric_name,
                           r.baseline_value, r.current_value, r.delta, f"{r.percent_change:.2f}"])
        
        # New functions
        for func in comparison['new_functions']:
            writer.writerow(['NEW', func, '', '', '', '', ''])
        
        # Removed functions
        for func in comparison['removed_functions']:
            writer.writerow(['REMOVED', func, '', '', '', '', ''])
    
    print(f"Comparison report exported to {filename}")


def export_comparison_json(comparison, filename):
    """Export comparison results to JSON file."""
    import json
    
    data = {
        'regressions': [{
            'function': r.func_name,
            'metric': r.metric_name,
            'baseline': r.baseline_value,
            'current': r.current_value,
            'delta': r.delta,
            'percent_change': r.percent_change
        } for r in comparison['regressions']],
        'improvements': [{
            'function': r.func_name,
            'metric': r.metric_name,
            'baseline': r.baseline_value,
            'current': r.current_value,
            'delta': r.delta,
            'percent_change': r.percent_change
        } for r in comparison['improvements']],
        'new_functions': comparison['new_functions'],
        'removed_functions': comparison['removed_functions']
    }
    
    with open(filename, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    
    print(f"Comparison report exported to {filename}")

