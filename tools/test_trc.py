#!/usr/bin/env python3
"""
Unified test suite for trace-scope tools
Run with: python test_trc.py
"""

import unittest
import tempfile
import os
import sys

# Import from trc.py
sys.path.insert(0, os.path.dirname(__file__))
import trc

# ============================================================================
# Test classes from test_trc_instrument.py
# ============================================================================

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
        result = trc.add_trace_scopes(code)
        # Should have exactly 1 TRC_SCOPE (in test_function, not in for loop)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
        
    def test_if_statement_not_instrumented(self):
        """Test that if statements are not instrumented."""
        code = """
void test_function() {
    if (condition) {
        do_something();
    }
}
"""
        result = trc.add_trace_scopes(code)
        # Should have exactly 1 TRC_SCOPE (in test_function, not in if)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
    def test_while_loop_not_instrumented(self):
        """Test that while loops are not instrumented."""
        code = """
void test_function() {
    while (running) {
        process();
    }
}
"""
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
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
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
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
        result = trc.add_trace_scopes(code)
        # Should have exactly 1 TRC_SCOPE (in test_function)
        # try and catch blocks should not be instrumented
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
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
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
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
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
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
        result = trc.add_trace_scopes(code)
        # Should have exactly 3 TRC_SCOPE calls (one per function)
        self.assertEqual(result.count('TRC_SCOPE()'), 3)


class TestFunctionDetection(unittest.TestCase):
    """Test that various function patterns are correctly detected."""
    
    def test_find_simple_function(self):
        """Test finding a simple free function."""
        code = """
void foo() {
    int x = 1;
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
        self.assertIn('foo', functions[0][2])
    
    def test_find_function_with_return_type(self):
        """Test finding function with return type."""
        code = """
int calculate(int a, int b) {
    return a + b;
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
        self.assertIn('calculate', functions[0][2])
    
    def test_find_function_complex_return_type(self):
        """Test finding function with complex return type."""
        code = """
std::vector<int> get_values() {
    return {1, 2, 3};
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
        
    def test_find_pointer_return_type(self):
        """Test finding function returning pointer."""
        code = """
const char* get_name() {
    return "test";
}
"""
        functions = trc.find_function_bodies(code)
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
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 2)
        function_names = [f[2] for f in functions]
        self.assertIn('method1', function_names)
        self.assertIn('method2', function_names)
    
    def test_find_constructor(self):
        """Test finding constructors."""
        code = """
class MyClass {
public:
    MyClass() {
        value = 0;
    }
    
    MyClass(int v) {
        value = v;
    }
};
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 2)
    
    def test_find_destructor(self):
        """Test finding destructors."""
        code = """
class MyClass {
public:
    ~MyClass() {
        cleanup();
    }
};
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
        self.assertIn('~MyClass', functions[0][2])
    
    def test_find_static_function(self):
        """Test finding static functions."""
        code = """
static void helper() {
    // static function
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_inline_function(self):
        """Test finding inline functions."""
        code = """
inline int fast_add(int a, int b) {
    return a + b;
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_virtual_function(self):
        """Test finding virtual functions."""
        code = """
class Base {
public:
    virtual void do_something() {
        // virtual function
    }
};
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_override_method(self):
        """Test finding override methods."""
        code = """
class Derived : public Base {
public:
    void do_something() override {
        // override
    }
};
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_const_method(self):
        """Test finding const methods."""
        code = """
class MyClass {
public:
    int get_value() const {
        return value;
    }
};
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_noexcept_function(self):
        """Test finding noexcept functions."""
        code = """
void safe_function() noexcept {
    // noexcept function
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_find_namespace_qualified(self):
        """Test finding namespace-qualified functions."""
        code = """
namespace MyNamespace {
    void function() {
        // namespace function
    }
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_template_function(self):
        """Test finding template functions (basic case)."""
        code = """
template<typename T>
T process(T value) {
    return value * 2;
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)


class TestTraceInstrumentation(unittest.TestCase):
    """Test the main instrumentation functionality."""
    
    def test_add_trace_scope(self):
        """Test adding TRC_SCOPE() to functions."""
        code = """
void foo() {
    int x = 1;
}
"""
        result = trc.add_trace_scopes(code)
        self.assertIn('TRC_SCOPE()', result)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
    def test_add_preserves_indentation(self):
        """Test that indentation is preserved."""
        code = """
void foo() {
    if (condition) {
        do_something();
    }
}
"""
        result = trc.add_trace_scopes(code)
        lines = result.split('\n')
        # Find the TRC_SCOPE line
        trc_line = None
        for line in lines:
            if 'TRC_SCOPE()' in line:
                trc_line = line
                break
        
        self.assertIsNotNone(trc_line)
        # Should have proper indentation (4 spaces)
        self.assertTrue(trc_line.startswith('    '))
    
    def test_add_multiple_functions(self):
        """Test adding to multiple functions."""
        code = """
void foo() {
    // function 1
}

void bar() {
    // function 2
}

void baz() {
    // function 3
}
"""
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 3)
    
    def test_remove_trace_scope(self):
        """Test removing TRC_SCOPE() calls."""
        code = """
void foo() {
    TRC_SCOPE();
    int x = 1;
}

void bar() {
    TRC_SCOPE();
    int y = 2;
}
"""
        result = trc.remove_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 0)
        self.assertNotIn('TRC_SCOPE()', result)
    
    def test_remove_preserves_other_code(self):
        """Test that remove doesn't affect other code."""
        code = """
void foo() {
    TRC_SCOPE();
    int x = 1;
    if (x > 0) {
        do_something();
    }
}
"""
        result = trc.remove_trace_scopes(code)
        self.assertNotIn('TRC_SCOPE()', result)
        self.assertIn('int x = 1;', result)
        self.assertIn('if (x > 0)', result)
    
    def test_roundtrip(self):
        """Test add then remove returns to original."""
        original = """
void foo() {
    int x = 1;
}
"""
        with_trace = trc.add_trace_scopes(original)
        back_to_original = trc.remove_trace_scopes(with_trace)
        
        # Should be back to original (minus the TRC_SCOPE line)
        self.assertNotIn('TRC_SCOPE()', back_to_original)
        self.assertIn('int x = 1;', back_to_original)
    
    def test_skip_already_instrumented(self):
        """Test that already instrumented functions are skipped."""
        code = """
void foo() {
    TRC_SCOPE();
    int x = 1;
}
"""
        result = trc.add_trace_scopes(code)
        # Should not add another TRC_SCOPE
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
    def test_nested_braces_in_function(self):
        """Test functions with nested braces."""
        code = """
void complex_function() {
    if (condition) {
        for (int i = 0; i < 10; i++) {
            do_something(i);
        }
    }
}
"""
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and special scenarios."""
    
    def test_empty_function(self):
        """Test function with empty body."""
        code = """
void empty() {
}
"""
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
    def test_one_liner_function(self):
        """Test one-line function."""
        code = """
int get_value() { return 42; }
"""
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
    def test_function_with_comments(self):
        """Test function with comments."""
        code = """
/**
 * This is a documented function
 */
void documented_function() {
    // Implementation here
    int x = 1;
}
"""
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
    def test_function_with_default_params(self):
        """Test function with default parameters."""
        code = """
void function_with_defaults(int x = 10, bool flag = true) {
    // function body
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_multiline_function_signature(self):
        """Test function with multi-line signature."""
        code = """
std::vector<int> 
process_data(
    const std::vector<int>& input,
    int multiplier
) {
    // function body
}
"""
        functions = trc.find_function_bodies(code)
        self.assertEqual(len(functions), 1)
    
    def test_lambda_expression_not_instrumented(self):
        """Test that lambda expressions are not instrumented."""
        code = """
void function_with_lambda() {
    auto lambda = []() {
        return 42;
    };
}
"""
        result = trc.add_trace_scopes(code)
        # Should only instrument the function, not the lambda
        self.assertEqual(result.count('TRC_SCOPE()'), 1)
    
    def test_multiple_classes(self):
        """Test multiple classes with methods."""
        code = """
class ClassA {
public:
    void methodA() {
        // method A
    }
};

class ClassB {
public:
    void methodB() {
        // method B
    }
};
"""
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 2)
    
    def test_no_functions_in_file(self):
        """Test file with no functions."""
        code = """
// Just comments and includes
#include <iostream>
#include <vector>

// Some constants
const int MAX_SIZE = 100;
"""
        result = trc.add_trace_scopes(code)
        self.assertEqual(result.count('TRC_SCOPE()'), 0)
    
    def test_real_world_example(self):
        """Test realistic C++ code."""
        code = """
#include <iostream>
#include <vector>

class DataProcessor {
public:
    DataProcessor() {
        // constructor
    }
    
    void process() {
        std::vector<int> data = {1, 2, 3, 4, 5};
        for (int value : data) {
            if (value % 2 == 0) {
                std::cout << value << std::endl;
            }
        }
    }
    
private:
    int count;
};

int calculate_sum(const std::vector<int>& numbers) {
    int sum = 0;
    for (int num : numbers) {
        sum += num;
    }
    return sum;
}

void standalone_function() {
    std::cout << "Hello, World!" << std::endl;
}
"""
        result = trc.add_trace_scopes(code)
        # Should instrument: DataProcessor constructor, process(), calculate_sum(), standalone_function()
        self.assertEqual(result.count('TRC_SCOPE()'), 4)


# ============================================================================
# Test classes for other functionality (placeholder for now)
# ============================================================================

class TestBinaryFormat(unittest.TestCase):
    """Test binary format reading functionality."""
    
    def test_read_header(self):
        """Test reading trace file header."""
        # This would need a real trace file to test properly
        # For now, just test that the function exists
        self.assertTrue(hasattr(trc, 'read_header'))
    
    def test_read_event(self):
        """Test reading individual events."""
        self.assertTrue(hasattr(trc, 'read_event'))
    
    def test_read_all_events(self):
        """Test reading all events from file."""
        self.assertTrue(hasattr(trc, 'read_all_events'))


class TestEventFilter(unittest.TestCase):
    """Test event filtering functionality."""
    
    def test_filter_creation(self):
        """Test creating an event filter."""
        filter_obj = trc.EventFilter()
        self.assertIsNotNone(filter_obj)
        self.assertEqual(len(filter_obj.include_functions), 0)
        self.assertEqual(len(filter_obj.exclude_functions), 0)
    
    def test_wildcard_match(self):
        """Test wildcard matching."""
        self.assertTrue(trc.wildcard_match("test*", "test_function"))
        self.assertTrue(trc.wildcard_match("*function", "test_function"))
        self.assertTrue(trc.wildcard_match("test*function", "test_my_function"))
        self.assertFalse(trc.wildcard_match("test*", "other_function"))
    
    def test_matches_any(self):
        """Test matches_any function."""
        patterns = ["test*", "*function"]
        self.assertTrue(trc.matches_any("test_function", patterns))
        self.assertTrue(trc.matches_any("my_function", patterns))
        self.assertFalse(trc.matches_any("other_method", patterns))


class TestCallGraph(unittest.TestCase):
    """Test call graph functionality."""
    
    def test_call_graph_creation(self):
        """Test creating a call graph."""
        graph = trc.CallGraph()
        self.assertIsNotNone(graph)
        self.assertEqual(len(graph.nodes), 0)
        self.assertEqual(len(graph.root_functions), 0)
    
    def test_add_edge(self):
        """Test adding edges to call graph."""
        graph = trc.CallGraph()
        graph.add_edge("caller", "callee", 1000)
        
        self.assertIn("caller", graph.nodes)
        self.assertIn("callee", graph.nodes)
    
    def test_build_call_graph(self):
        """Test building call graph from events."""
        # Mock events for testing
        events = [
            {'type': trc.EVENT_TYPE_ENTER, 'func': 'main', 'ts_ns': 0},
            {'type': trc.EVENT_TYPE_ENTER, 'func': 'foo', 'ts_ns': 1000},
            {'type': trc.EVENT_TYPE_EXIT, 'func': 'foo', 'ts_ns': 2000},
            {'type': trc.EVENT_TYPE_EXIT, 'func': 'main', 'ts_ns': 3000}
        ]
        
        graph = trc.build_call_graph(events)
        self.assertIsNotNone(graph)
        self.assertIn('main', graph.nodes)
        self.assertIn('foo', graph.nodes)


class TestStatistics(unittest.TestCase):
    """Test statistics computation."""
    
    def test_compute_stats(self):
        """Test computing statistics from events."""
        # Mock events for testing
        events = [
            {'type': trc.EVENT_TYPE_ENTER, 'func': 'main', 'ts_ns': 0, 'depth': 0, 'tid': 1, 'file': 'test.cpp', 'line': 10},
            {'type': trc.EVENT_TYPE_EXIT, 'func': 'main', 'ts_ns': 1000000, 'dur_ns': 1000000, 'depth': 0, 'memory_rss': 1024, 'tid': 1, 'file': 'test.cpp', 'line': 15}
        ]
        
        filter_obj = trc.EventFilter()
        global_stats, thread_stats = trc.compute_stats(events, filter_obj)
        
        self.assertIsNotNone(global_stats)
        self.assertIsNotNone(thread_stats)
        self.assertIn('main', global_stats)


class TestFormatting(unittest.TestCase):
    """Test formatting utilities."""
    
    def test_format_duration(self):
        """Test duration formatting."""
        self.assertEqual(trc.format_duration(500), "500 ns")
        self.assertEqual(trc.format_duration(1500), "1.50 us")
        self.assertEqual(trc.format_duration(1500000), "1.50 ms")
        self.assertEqual(trc.format_duration(1500000000), "1.500 s")
    
    def test_format_memory(self):
        """Test memory formatting."""
        self.assertEqual(trc.format_memory(512), "512 B")
        self.assertEqual(trc.format_memory(1536), "1.50 KB")
        self.assertEqual(trc.format_memory(1536000), "1.46 MB")
        self.assertEqual(trc.format_memory(1536000000), "1.43 GB")
    
    def test_get_color(self):
        """Test color code generation."""
        event = {'depth': 0, 'color_offset': 0}
        start, end = trc.get_color(event, True)
        self.assertIsInstance(start, str)
        self.assertIsInstance(end, str)
        
        # Test with color disabled
        start, end = trc.get_color(event, False)
        self.assertEqual(start, '')
        self.assertEqual(end, '')


if __name__ == '__main__':
    unittest.main()
