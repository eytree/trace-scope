#!/usr/bin/env python3
"""Test trc_compare.py - Performance regression detection."""

import sys
import os

# Add tools directory to path
sys.path.insert(0, os.path.dirname(__file__))

from trc_compare import Regression, compare_traces
from trc_common import EVENT_TYPE_ENTER, EVENT_TYPE_EXIT, EventFilter
import tempfile
import struct


def write_simple_trace(filename, function_stats):
    """
    Write a simple trace file with specified function statistics.
    
    Args:
        filename: Output filename
        function_stats: dict of func_name -> (calls, avg_duration_ns, memory_rss)
    """
    with open(filename, 'wb') as f:
        # Write header
        f.write(b'TRCLOG10')
        f.write(struct.pack('<II', 2, 0))  # version 2, padding
        
        # Write events for each function
        tid = 1
        ts = 1000
        
        for func, (calls, dur, mem) in function_stats.items():
            for _ in range(calls):
                # Enter event
                f.write(struct.pack('<B', EVENT_TYPE_ENTER))  # type
                f.write(struct.pack('<I', tid))  # tid
                f.write(struct.pack('<B', 0))  # color_offset
                f.write(struct.pack('<Q', ts))  # ts_ns
                f.write(struct.pack('<I', 0))  # depth
                f.write(struct.pack('<Q', 0))  # dur_ns
                f.write(struct.pack('<Q', mem))  # memory_rss
                f.write(struct.pack('<H', 0))  # file_len
                f.write(struct.pack('<H', len(func)))  # func_len
                f.write(func.encode('utf-8'))
                f.write(struct.pack('<H', 0))  # msg_len
                f.write(struct.pack('<I', 10))  # line
                
                ts += 100
                
                # Exit event
                f.write(struct.pack('<B', EVENT_TYPE_EXIT))  # type
                f.write(struct.pack('<I', tid))  # tid
                f.write(struct.pack('<B', 0))  # color_offset
                f.write(struct.pack('<Q', ts))  # ts_ns
                f.write(struct.pack('<I', 0))  # depth
                f.write(struct.pack('<Q', dur))  # dur_ns
                f.write(struct.pack('<Q', mem))  # memory_rss
                f.write(struct.pack('<H', 0))  # file_len
                f.write(struct.pack('<H', len(func)))  # func_len
                f.write(func.encode('utf-8'))
                f.write(struct.pack('<H', 0))  # msg_len
                f.write(struct.pack('<I', 10))  # line
                
                ts += dur + 100


def test_regression_class():
    """Test Regression class."""
    print("Testing Regression class...")
    
    # Test regression (slower)
    r = Regression("slow_func", "avg_ns", 1000, 2000)
    assert r.func_name == "slow_func"
    assert r.metric_name == "avg_ns"
    assert r.baseline_value == 1000
    assert r.current_value == 2000
    assert r.delta == 1000
    assert abs(r.percent_change - 100.0) < 0.1  # 100% increase
    assert r.is_regression == True
    
    # Test improvement (faster)
    r = Regression("fast_func", "avg_ns", 2000, 1000)
    assert r.delta == -1000
    assert abs(r.percent_change - (-50.0)) < 0.1  # 50% decrease
    assert r.is_regression == False  # It's an improvement
    
    # Test call count change
    r = Regression("func", "calls", 10, 20)
    assert r.percent_change == 100.0
    
    print("[PASS] Regression class tests passed")


def test_compare_traces():
    """Test trace comparison."""
    print("Testing compare_traces...")
    
    # Create temporary files
    with tempfile.NamedTemporaryFile(mode='wb', delete=False, suffix='_baseline.bin') as f:
        baseline_file = f.name
    with tempfile.NamedTemporaryFile(mode='wb', delete=False, suffix='_current.bin') as f:
        current_file = f.name
    
    try:
        # Baseline: func_a is slow, func_b exists
        baseline_stats = {
            'func_a': (5, 10000000, 1024*1024),  # 5 calls, 10ms avg, 1MB memory
            'func_b': (10, 5000000, 512*1024),   # 10 calls, 5ms avg, 512KB memory
            'removed_func': (2, 1000000, 0),     # Will be removed
        }
        write_simple_trace(baseline_file, baseline_stats)
        
        # Current: func_a is slower (regression), func_c is new
        current_stats = {
            'func_a': (5, 20000000, 2048*1024),  # Same calls, 2x slower, 2x memory
            'func_b': (10, 2500000, 512*1024),   # Same calls, 2x faster (improvement)
            'new_func': (3, 3000000, 0),         # New function
        }
        write_simple_trace(current_file, current_stats)
        
        # Compare
        comparison = compare_traces(baseline_file, current_file, threshold=10.0)
        
        # Check regressions
        assert len(comparison['regressions']) > 0, "Should detect regressions"
        
        # Find func_a regressions
        func_a_regressions = [r for r in comparison['regressions'] if r.func_name == 'func_a']
        assert len(func_a_regressions) >= 2, "Should have avg_ns and total_ns regressions for func_a"
        
        # Check improvements
        assert len(comparison['improvements']) > 0, "Should detect improvements"
        func_b_improvements = [r for r in comparison['improvements'] if r.func_name == 'func_b']
        assert len(func_b_improvements) >= 2, "Should have improvements for func_b"
        
        # Check new/removed
        assert 'new_func' in comparison['new_functions'], "Should detect new function"
        assert 'removed_func' in comparison['removed_functions'], "Should detect removed function"
        
        print("[PASS] compare_traces tests passed")
    
    finally:
        # Clean up
        try:
            os.unlink(baseline_file)
            os.unlink(current_file)
        except:
            pass


def test_threshold_filtering():
    """Test threshold filtering."""
    print("Testing threshold filtering...")
    
    with tempfile.NamedTemporaryFile(mode='wb', delete=False, suffix='_baseline.bin') as f:
        baseline_file = f.name
    with tempfile.NamedTemporaryFile(mode='wb', delete=False, suffix='_current.bin') as f:
        current_file = f.name
    
    try:
        # Small change (3%)
        baseline_stats = {'func': (1, 1000000, 0)}  # 1ms
        current_stats = {'func': (1, 1030000, 0)}   # 1.03ms (3% increase)
        
        write_simple_trace(baseline_file, baseline_stats)
        write_simple_trace(current_file, current_stats)
        
        # With 5% threshold, should not report
        comparison = compare_traces(baseline_file, current_file, threshold=5.0)
        assert len(comparison['regressions']) == 0, "3% change below 5% threshold"
        
        # With 2% threshold, should report
        comparison = compare_traces(baseline_file, current_file, threshold=2.0)
        assert len(comparison['regressions']) > 0, "3% change above 2% threshold"
        
        print("[PASS] Threshold filtering tests passed")
    
    finally:
        try:
            os.unlink(baseline_file)
            os.unlink(current_file)
        except:
            pass


def main():
    print("=" * 70)
    print(" Testing trc_compare.py")
    print("=" * 70)
    print()
    
    test_regression_class()
    test_compare_traces()
    test_threshold_filtering()
    
    print()
    print("=" * 70)
    print(" All tests passed!")
    print("=" * 70)


if __name__ == '__main__':
    main()

