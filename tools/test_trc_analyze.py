#!/usr/bin/env python3
"""Test trc_analyze.py and trc_common.py - Wildcard matching and filtering."""

import sys
import os

# Add tools directory to path
sys.path.insert(0, os.path.dirname(__file__))

from trc_common import wildcard_match, matches_any, EventFilter


def test_wildcard():
    """Test wildcard matching."""
    print("Testing wildcard matching...")
    
    # Exact match
    assert wildcard_match("test", "test"), "Exact match failed"
    assert not wildcard_match("test", "testing"), "Should not match suffix"
    assert not wildcard_match("test", "tes"), "Should not match prefix"
    
    # Star suffix
    assert wildcard_match("test_*", "test_"), "test_* should match test_"
    assert wildcard_match("test_*", "test_foo"), "test_* should match test_foo"
    assert wildcard_match("test_*", "test_bar_baz"), "test_* should match test_bar_baz"
    assert not wildcard_match("test_*", "testing"), "test_* should not match testing"
    assert not wildcard_match("test_*", "my_test"), "test_* should not match my_test"
    
    # Star prefix
    assert wildcard_match("*_test", "my_test"), "*_test should match my_test"
    assert wildcard_match("*_test", "foo_bar_test"), "*_test should match foo_bar_test"
    assert wildcard_match("*_test", "_test"), "*_test should match _test"
    assert not wildcard_match("*_test", "test"), "*_test should not match test"
    assert not wildcard_match("*_test", "testing"), "*_test should not match testing"
    
    # Star middle
    assert wildcard_match("*mid*", "mid"), "*mid* should match mid"
    assert wildcard_match("*mid*", "middle"), "*mid* should match middle"
    assert wildcard_match("*mid*", "pyramid"), "*mid* should match pyramid"
    assert wildcard_match("*mid*", "amid"), "*mid* should match amid"
    assert not wildcard_match("*mid*", "test"), "*mid* should not match test"
    
    # Star only
    assert wildcard_match("*", "anything"), "* should match anything"
    assert wildcard_match("*", "test_foo"), "* should match test_foo"
    assert wildcard_match("*", ""), "* should match empty"
    
    # Multiple stars
    assert wildcard_match("*::*", "namespace::function"), "*::* should match namespace::function"
    assert wildcard_match("test_*_*", "test_foo_bar"), "test_*_* should match test_foo_bar"
    
    print("[PASS] Wildcard matching tests passed")


def test_matches_any():
    """Test matches_any function."""
    print("Testing matches_any...")
    
    patterns = ["test_*", "core_*"]
    assert matches_any("test_foo", patterns), "Should match test_*"
    assert matches_any("core_process", patterns), "Should match core_*"
    assert not matches_any("other_func", patterns), "Should not match any pattern"
    
    # Empty patterns
    assert not matches_any("test_foo", []), "Should not match empty patterns"
    
    # Empty text
    assert not matches_any("", patterns), "Empty text should not match"
    
    print("[PASS] matches_any tests passed")


def test_filtering():
    """Test event filtering logic."""
    print("Testing EventFilter...")
    
    filt = EventFilter()
    filt.include_functions = ["core_*"]
    filt.exclude_functions = ["*_test"]
    filt.max_depth = 5
    
    # Should pass all filters
    event = {
        'func': 'core_process',
        'file': 'main.cpp',
        'depth': 3,
        'tid': 0x1234
    }
    assert filt.should_trace(event), "Should pass all filters"
    
    # Exclude function
    event['func'] = 'core_test'
    assert not filt.should_trace(event), "Should be excluded (matches *_test)"
    
    # Not in include list
    event['func'] = 'other_func'
    assert not filt.should_trace(event), "Should not match include list"
    
    # Depth too deep
    event['func'] = 'core_func'
    event['depth'] = 10
    assert not filt.should_trace(event), "Should be filtered by depth"
    
    # Back to passing
    event['depth'] = 2
    assert filt.should_trace(event), "Should pass again"
    
    print("[PASS] EventFilter tests passed")


def test_file_filtering():
    """Test file filtering."""
    print("Testing file filtering...")
    
    filt = EventFilter()
    filt.include_files = ["src/core/*"]
    filt.exclude_files = ["*/test/*"]
    
    event = {
        'func': 'func',
        'file': 'src/core/main.cpp',
        'depth': 0,
        'tid': 0x1234
    }
    assert filt.should_trace(event), "Should match src/core/*"
    
    event['file'] = 'src/test/main.cpp'
    assert not filt.should_trace(event), "Should be excluded (*/test/*)"
    
    event['file'] = 'lib/other.cpp'
    assert not filt.should_trace(event), "Should not match include list"
    
    print("[PASS] File filtering tests passed")


def test_thread_filtering():
    """Test thread filtering."""
    print("Testing thread filtering...")
    
    filt = EventFilter()
    filt.include_threads = [0x1234, 0x5678]
    
    event = {
        'func': 'func',
        'file': 'main.cpp',
        'depth': 0,
        'tid': 0x1234
    }
    assert filt.should_trace(event), "Should match included thread"
    
    event['tid'] = 0x9999
    assert not filt.should_trace(event), "Should not match thread list"
    
    # Test exclude
    filt2 = EventFilter()
    filt2.exclude_threads = [0x1234]
    
    event['tid'] = 0x1234
    assert not filt2.should_trace(event), "Should be excluded"
    
    event['tid'] = 0x5678
    assert filt2.should_trace(event), "Should pass (not excluded)"
    
    print("[PASS] Thread filtering tests passed")


def test_empty_filters():
    """Test that empty filters pass everything."""
    print("Testing empty filters...")
    
    filt = EventFilter()
    
    event = {
        'func': 'any_function',
        'file': 'any_file.cpp',
        'depth': 100,
        'tid': 0xFFFFFFFF
    }
    
    assert filt.should_trace(event), "Empty filters should pass everything"
    
    print("[PASS] Empty filter tests passed")


def test_exclude_wins():
    """Test that exclude takes priority over include."""
    print("Testing exclude priority...")
    
    filt = EventFilter()
    filt.include_functions = ["test_*"]
    filt.exclude_functions = ["test_foo"]
    
    event = {
        'func': 'test_foo',
        'file': 'test.cpp',
        'depth': 0,
        'tid': 0x1234
    }
    
    # Matches include (test_*) but also matches exclude (test_foo)
    # Exclude should win
    assert not filt.should_trace(event), "Exclude should win over include"
    
    event['func'] = 'test_bar'
    assert filt.should_trace(event), "Should match include and not excluded"
    
    print("[PASS] Exclude priority tests passed")


def main():
    """Run all tests."""
    print("=======================================================================")
    print(" Testing trc_pretty.py v2")
    print("=======================================================================\n")
    
    try:
        test_wildcard()
        test_matches_any()
        test_filtering()
        test_file_filtering()
        test_thread_filtering()
        test_empty_filters()
        test_exclude_wins()
        
        print("\n=======================================================================")
        print(" All tests passed!")
        print("=======================================================================")
        return 0
        
    except AssertionError as e:
        print(f"\n[FAIL] Test failed: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"\n[ERROR] Unexpected error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    sys.exit(main())

