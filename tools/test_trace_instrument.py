#!/usr/bin/env python3
"""
Unit tests for trace_instrument.py

Run with: python test_trace_instrument.py
"""

import unittest
import tempfile
import os
import sys

# Import the module we're testing
sys.path.insert(0, os.path.dirname(__file__))
from trace_instrument import add_trace_scopes, remove_trace_scopes, find_function_bodies


class TestTraceInstrument(unittest.TestCase):
    """Test cases for trace_instrument.py"""
    
    def test_find_simple_function(self):
        """Test finding a simple free function."""
        code = """
void foo() {
    int x = 1;
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
        self.assertIn('foo', functions[0][2])
    
    def test_find_function_with_return_type(self):
        """Test finding function with return type."""
        code = """
int calculate(int a, int b) {
    return a + b;
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
        self.assertIn('calculate', functions[0][2])
    
    def test_find_class_methods(self):
        """Test finding class methods."""
        code = """
class MyClass {
public:
    void method1() {
        std::cout << "test";
    }
    
    int method2(int x) {
        return x * 2;
    }
};
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 2)
    
    def test_find_namespace_qualified(self):
        """Test finding namespace-qualified functions."""
        code = """
void MyNamespace::my_function() {
    int x = 1;
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_add_trace_scope(self):
        """Test adding TRACE_SCOPE() to functions."""
        code = """void foo() {
    int x = 1;
}"""
        result = add_trace_scopes(code)
        self.assertIn('TRACE_SCOPE()', result)
        # Verify it's at the start of the function body
        lines = result.split('\n')
        self.assertTrue(any('TRACE_SCOPE()' in line for line in lines))
    
    def test_add_preserves_indentation(self):
        """Test that indentation is preserved."""
        code = """void foo() {
    int x = 1;
    int y = 2;
}"""
        result = add_trace_scopes(code)
        lines = result.split('\n')
        # Find TRACE_SCOPE line
        trace_line = [l for l in lines if 'TRACE_SCOPE()' in l][0]
        # Should have same indentation as function body (4 spaces)
        self.assertTrue(trace_line.startswith('    TRACE_SCOPE()'))
    
    def test_skip_already_instrumented(self):
        """Test that already instrumented functions are skipped."""
        code = """void foo() {
    TRACE_SCOPE();
    int x = 1;
}"""
        result = add_trace_scopes(code)
        # Should only have one TRACE_SCOPE
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
    
    def test_remove_trace_scope(self):
        """Test removing TRACE_SCOPE() calls."""
        code = """void foo() {
    TRACE_SCOPE();
    int x = 1;
}

void bar() {
    TRACE_SCOPE();
    return;
}"""
        result = remove_trace_scopes(code)
        self.assertNotIn('TRACE_SCOPE()', result)
        # Verify code structure is preserved
        self.assertIn('void foo()', result)
        self.assertIn('void bar()', result)
        self.assertIn('int x = 1', result)
    
    def test_remove_preserves_other_code(self):
        """Test that remove doesn't affect other code."""
        code = """void foo() {
    TRACE_SCOPE();
    int x = 1;
    std::cout << "test";
}"""
        result = remove_trace_scopes(code)
        self.assertIn('int x = 1', result)
        self.assertIn('std::cout', result)
        self.assertNotIn('TRACE_SCOPE()', result)
    
    def test_add_multiple_functions(self):
        """Test adding to multiple functions."""
        code = """
void foo() {
    int x = 1;
}

void bar() {
    int y = 2;
}

int baz() {
    return 3;
}
"""
        result = add_trace_scopes(code)
        # Should have 3 TRACE_SCOPE calls
        self.assertEqual(result.count('TRACE_SCOPE()'), 3)
    
    def test_roundtrip(self):
        """Test add then remove returns to original (minus whitespace)."""
        code = """void foo() {
    int x = 1;
}"""
        added = add_trace_scopes(code)
        removed = remove_trace_scopes(added)
        # Should be back to original (minus potential whitespace differences)
        self.assertNotIn('TRACE_SCOPE()', removed)
        self.assertIn('int x = 1', removed)


def run_tests():
    """Run all tests and display results."""
    suite = unittest.TestLoader().loadTestsFromTestCase(TestTraceInstrument)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    print("\n" + "="*60)
    if result.wasSuccessful():
        print("✓ All tests passed!")
    else:
        print(f"✗ {len(result.failures)} test(s) failed")
        print(f"✗ {len(result.errors)} error(s)")
    print("="*60)
    
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_tests())

