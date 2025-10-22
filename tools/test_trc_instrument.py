#!/usr/bin/env python3
"""
Unit tests for trc_instrument.py

Run with: python test_trc_instrument.py
"""

import unittest
import tempfile
import os
import sys

# Import the module we're testing
sys.path.insert(0, os.path.dirname(__file__))
from trc_instrument import add_trace_scopes, remove_trace_scopes, find_function_bodies, is_control_flow_statement


class TestControlFlowDetection(unittest.TestCase):
    """Test that control flow statements are correctly identified and excluded."""
    
    def test_for_loop_not_instrumented(self):
        """Test that for loops are not instrumented."""
        code = """
void test_function() {
    for (int i = 0; i < 10; i++) {
        std::cout << i;
    }
}
"""
        result = add_trace_scopes(code)
        # Should have exactly 1 TRACE_SCOPE (in test_function, not in for loop)
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
        
    def test_if_statement_not_instrumented(self):
        """Test that if statements are not instrumented."""
        code = """
void test_function() {
    if (condition) {
        do_something();
    }
}
"""
        result = add_trace_scopes(code)
        # Should have exactly 1 TRACE_SCOPE (in test_function, not in if)
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
    
    def test_while_loop_not_instrumented(self):
        """Test that while loops are not instrumented."""
        code = """
void test_function() {
    while (running) {
        process();
    }
}
"""
        result = add_trace_scopes(code)
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
    
    def test_switch_statement_not_instrumented(self):
        """Test that switch statements are not instrumented."""
        code = """
void test_function(int x) {
    switch (x) {
        case 1:
            break;
        default:
            break;
    }
}
"""
        result = add_trace_scopes(code)
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
    
    def test_catch_block_not_instrumented(self):
        """Test that catch blocks are not instrumented."""
        code = """
void test_function() {
    try {
        risky_operation();
    } catch (const std::exception& e) {
        handle_error();
    }
}
"""
        result = add_trace_scopes(code)
        # Should have exactly 1 TRACE_SCOPE (in test_function)
        # try and catch blocks should not be instrumented
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
    
    def test_else_if_not_instrumented(self):
        """Test that else if statements are not instrumented."""
        code = """
void test_function(int x) {
    if (x > 0) {
        positive();
    } else if (x < 0) {
        negative();
    } else {
        zero();
    }
}
"""
        result = add_trace_scopes(code)
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
    
    def test_do_while_not_instrumented(self):
        """Test that do-while loops are not instrumented."""
        code = """
void test_function() {
    int i = 0;
    do {
        process(i++);
    } while (i < 10);
}
"""
        result = add_trace_scopes(code)
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
    
    def test_mixed_control_flow(self):
        """Test file with multiple control flow statements and functions."""
        code = """
void function_one() {
    for (int i = 0; i < 5; i++) {
        if (i % 2 == 0) {
            process(i);
        }
    }
}

int function_two(int x) {
    while (x > 0) {
        x--;
    }
    return x;
}

void function_three() {
    switch (mode) {
        case 1:
            break;
    }
}
"""
        result = add_trace_scopes(code)
        # Should have exactly 3 TRACE_SCOPE calls (one per function)
        self.assertEqual(result.count('TRACE_SCOPE()'), 3)


class TestFunctionDetection(unittest.TestCase):
    """Test that various function patterns are correctly detected."""
    
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
    
    def test_find_function_complex_return_type(self):
        """Test finding function with complex return type."""
        code = """
std::vector<int> get_values() {
    return {1, 2, 3};
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
        
    def test_find_pointer_return_type(self):
        """Test finding function returning pointer."""
        code = """
const char* get_name() {
    return "test";
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
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
        self.assertIn('MyNamespace::my_function', functions[0][2])
    
    def test_find_constructor(self):
        """Test finding constructors."""
        code = """
MyClass::MyClass() {
    initialize();
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_destructor(self):
        """Test finding destructors."""
        code = """
MyClass::~MyClass() {
    cleanup();
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
        self.assertIn('~MyClass', functions[0][2])
    
    def test_find_const_method(self):
        """Test finding const methods."""
        code = """
int MyClass::get_value() const {
    return value;
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_override_method(self):
        """Test finding override methods."""
        code = """
void MyClass::do_something() override {
    base_action();
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_noexcept_function(self):
        """Test finding noexcept functions."""
        code = """
void safe_function() noexcept {
    // cannot throw
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_inline_function(self):
        """Test finding inline functions."""
        code = """
inline int fast_calc(int x) {
    return x * 2;
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_static_function(self):
        """Test finding static functions."""
        code = """
static void helper() {
    // internal
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_virtual_function(self):
        """Test finding virtual functions."""
        code = """
virtual void process() {
    // override me
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_template_function(self):
        """Test finding template functions (basic case)."""
        code = """
template<typename T>
T max_value(T a, T b) {
    return a > b ? a : b;
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)


class TestTraceInstrumentation(unittest.TestCase):
    """Test adding and removing TRACE_SCOPE() calls."""
    
    def test_add_trace_scope(self):
        """Test adding TRACE_SCOPE() to functions."""
        code = """void foo() {
    int x = 1;
}"""
        result = add_trace_scopes(code)
        self.assertIn('TRACE_SCOPE()', result)
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
        """Test add then remove returns to original."""
        code = """void foo() {
    int x = 1;
}"""
        added = add_trace_scopes(code)
        removed = remove_trace_scopes(added)
        self.assertNotIn('TRACE_SCOPE()', removed)
        self.assertIn('int x = 1', removed)
    
    def test_nested_braces_in_function(self):
        """Test functions with nested braces."""
        code = """
void complex_function() {
    if (condition) {
        int arr[] = {1, 2, 3};
        for (int x : arr) {
            process(x);
        }
    }
}
"""
        result = add_trace_scopes(code)
        # Should have exactly 1 TRACE_SCOPE (in the function)
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and corner scenarios."""
    
    def test_empty_function(self):
        """Test function with empty body."""
        code = """void empty() {
}"""
        result = add_trace_scopes(code)
        self.assertIn('TRACE_SCOPE()', result)
    
    def test_one_liner_function(self):
        """Test one-line function."""
        code = """int get_value() { return 42; }"""
        result = add_trace_scopes(code)
        self.assertIn('TRACE_SCOPE()', result)
    
    def test_function_with_default_params(self):
        """Test function with default parameters."""
        code = """
void configure(int value = 10, bool flag = true) {
    setup(value, flag);
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_multiline_function_signature(self):
        """Test function with multi-line signature."""
        code = """
int calculate_sum(
    int a,
    int b,
    int c) {
    return a + b + c;
}
"""
        functions = find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_function_with_comments(self):
        """Test function with comments."""
        code = """
// This is a function
void documented_function() {
    // Do work
    work();
}
"""
        result = add_trace_scopes(code)
        self.assertIn('TRACE_SCOPE()', result)
    
    def test_multiple_classes(self):
        """Test multiple classes with methods."""
        code = """
class ClassA {
public:
    void methodA() {
        doA();
    }
};

class ClassB {
public:
    void methodB() {
        doB();
    }
};
"""
        result = add_trace_scopes(code)
        self.assertEqual(result.count('TRACE_SCOPE()'), 2)
    
    def test_lambda_expression_not_instrumented(self):
        """Test that lambda expressions are not instrumented."""
        code = """
void function_with_lambda() {
    auto lambda = [](int x) {
        return x * 2;
    };
    int result = lambda(5);
}
"""
        result = add_trace_scopes(code)
        # Should have exactly 1 TRACE_SCOPE (in function_with_lambda)
        # Lambda should not be instrumented
        self.assertEqual(result.count('TRACE_SCOPE()'), 1)
    
    def test_no_functions_in_file(self):
        """Test file with no functions."""
        code = """
// Just some comments
int global_var = 42;
#define MACRO_VALUE 100
"""
        result = add_trace_scopes(code)
        # Should not add any TRACE_SCOPE
        self.assertNotIn('TRACE_SCOPE()', result)
    
    def test_real_world_example(self):
        """Test realistic C++ code."""
        code = """
#include <iostream>
#include <vector>

class DataProcessor {
private:
    std::vector<int> data;

public:
    DataProcessor() {
        data.reserve(100);
    }
    
    void process() {
        for (const auto& item : data) {
            if (item > 0) {
                std::cout << item << std::endl;
            }
        }
    }
    
    int calculate_sum() const {
        int sum = 0;
        for (int val : data) {
            sum += val;
        }
        return sum;
    }
};

void standalone_function(int x) {
    switch (x) {
        case 1:
            std::cout << "one";
            break;
        default:
            std::cout << "other";
    }
}
"""
        result = add_trace_scopes(code)
        # Should have 4 TRACE_SCOPE calls:
        # 1. DataProcessor() constructor
        # 2. process() method
        # 3. calculate_sum() method
        # 4. standalone_function()
        self.assertEqual(result.count('TRACE_SCOPE()'), 4)


def run_tests():
    """Run all tests and display results."""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    # Add all test classes
    suite.addTests(loader.loadTestsFromTestCase(TestControlFlowDetection))
    suite.addTests(loader.loadTestsFromTestCase(TestFunctionDetection))
    suite.addTests(loader.loadTestsFromTestCase(TestTraceInstrumentation))
    suite.addTests(loader.loadTestsFromTestCase(TestEdgeCases))
    
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
