#!/usr/bin/env python3
"""Test trc_diff.py - Trace diff and execution path comparison."""

import sys
import os

# Add tools directory to path
sys.path.insert(0, os.path.dirname(__file__))

from trc_diff import ExecutionPath, compute_diff
from trc_common import EVENT_TYPE_ENTER, EVENT_TYPE_EXIT


def test_execution_path():
    """Test ExecutionPath class."""
    print("Testing ExecutionPath...")
    
    events = [
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'main', 'ts_ns': 1000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'foo', 'ts_ns': 2000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'foo', 'ts_ns': 3000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 1000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'main', 'ts_ns': 4000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 3000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
    ]
    
    path = ExecutionPath(events)
    
    assert len(path.call_sequence) == 2, "Should have 2 enter events"
    assert path.call_sequence[0] == ('main', 0), "First call should be main at depth 0"
    assert path.call_sequence[1] == ('foo', 1), "Second call should be foo at depth 1"
    assert 'main' in path.function_set, "Function set should contain main"
    assert 'foo' in path.function_set, "Function set should contain foo"
    
    print("[PASS] ExecutionPath tests passed")


def test_identical_traces():
    """Test diff of identical traces."""
    print("Testing identical traces...")
    
    events = [
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'main', 'ts_ns': 1000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'main', 'ts_ns': 2000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 1000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
    ]
    
    path_a = ExecutionPath(events)
    path_b = ExecutionPath(events)  # Same events
    
    diff = compute_diff(path_a, path_b)
    
    assert len(diff['only_in_a']) == 0, "No functions should be only in A"
    assert len(diff['only_in_b']) == 0, "No functions should be only in B"
    assert len(diff['common']) == 1, "Should have 1 common function"
    assert 'main' in diff['common'], "main should be common"
    assert len(diff['sequence_diff']) == 0, "No sequence differences"
    
    print("[PASS] Identical traces tests passed")


def test_different_functions():
    """Test diff with different functions."""
    print("Testing different functions...")
    
    events_a = [
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'func_a', 'ts_ns': 1000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'common', 'ts_ns': 2000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'common', 'ts_ns': 3000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 1000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'func_a', 'ts_ns': 4000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 3000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
    ]
    
    events_b = [
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'func_b', 'ts_ns': 1000, 'depth': 0,
         'file': 'test.cpp', 'line': 30, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'common', 'ts_ns': 2000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'common', 'ts_ns': 3000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 1000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'func_b', 'ts_ns': 4000, 'depth': 0,
         'file': 'test.cpp', 'line': 30, 'dur_ns': 3000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
    ]
    
    path_a = ExecutionPath(events_a)
    path_b = ExecutionPath(events_b)
    
    diff = compute_diff(path_a, path_b)
    
    assert len(diff['only_in_a']) == 1, "Should have 1 function only in A"
    assert 'func_a' in diff['only_in_a'], "func_a should be only in A"
    
    assert len(diff['only_in_b']) == 1, "Should have 1 function only in B"
    assert 'func_b' in diff['only_in_b'], "func_b should be only in B"
    
    assert len(diff['common']) == 1, "Should have 1 common function"
    assert 'common' in diff['common'], "common should be in both"
    
    assert len(diff['sequence_diff']) > 0, "Should have sequence differences"
    
    print("[PASS] Different functions tests passed")


def test_sequence_diff():
    """Test sequence difference detection."""
    print("Testing sequence differences...")
    
    # Same functions, different order
    events_a = [
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'main', 'ts_ns': 1000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'foo', 'ts_ns': 2000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'foo', 'ts_ns': 3000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 1000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'bar', 'ts_ns': 4000, 'depth': 1,
         'file': 'test.cpp', 'line': 30, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'bar', 'ts_ns': 5000, 'depth': 1,
         'file': 'test.cpp', 'line': 30, 'dur_ns': 1000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'main', 'ts_ns': 6000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 5000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
    ]
    
    events_b = [
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'main', 'ts_ns': 1000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'bar', 'ts_ns': 2000, 'depth': 1,
         'file': 'test.cpp', 'line': 30, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'bar', 'ts_ns': 3000, 'depth': 1,
         'file': 'test.cpp', 'line': 30, 'dur_ns': 1000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_ENTER, 'tid': 1, 'func': 'foo', 'ts_ns': 4000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 0, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'foo', 'ts_ns': 5000, 'depth': 1,
         'file': 'test.cpp', 'line': 20, 'dur_ns': 1000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
        {'type': EVENT_TYPE_EXIT, 'tid': 1, 'func': 'main', 'ts_ns': 6000, 'depth': 0,
         'file': 'test.cpp', 'line': 10, 'dur_ns': 5000, 'color_offset': 0, 'memory_rss': 0, 'msg': ''},
    ]
    
    path_a = ExecutionPath(events_a)
    path_b = ExecutionPath(events_b)
    
    diff = compute_diff(path_a, path_b)
    
    # Same functions, but different order
    assert len(diff['only_in_a']) == 0, "No functions only in A"
    assert len(diff['only_in_b']) == 0, "No functions only in B"
    assert len(diff['common']) == 3, "All 3 functions are common"
    assert len(diff['sequence_diff']) > 0, "Should detect sequence difference (foo and bar swapped)"
    
    print("[PASS] Sequence difference tests passed")


def main():
    print("=" * 70)
    print(" Testing trc_diff.py")
    print("=" * 70)
    print()
    
    test_execution_path()
    test_identical_traces()
    test_different_functions()
    test_sequence_diff()
    
    print()
    print("=" * 70)
    print(" All tests passed!")
    print("=" * 70)


if __name__ == '__main__':
    main()

