/**
 * @file test_framework_test.cpp
 * @brief Self-test for the test framework.
 * 
 * Tests the test framework itself to ensure:
 * - Test registration works
 * - Assertions work correctly
 * - Test execution works
 * - Pass/fail reporting works
 */

#include "test_framework.hpp"
#include <cstring>

// Test 1: Basic assertion that should pass
TEST(assertion_pass) {
    TEST_ASSERT(true, "true should be true");
    TEST_ASSERT(1 + 1 == 2, "Basic arithmetic");
    TEST_ASSERT(std::strcmp("hello", "hello") == 0, "String comparison");
}

// Test 2: Equality assertion
TEST(assertion_eq) {
    int a = 42;
    int b = 42;
    TEST_ASSERT_EQ(a, b, "Integers should be equal");
    TEST_ASSERT_EQ(5 + 5, 10, "Arithmetic result");
}

// Test 3: Inequality assertion
TEST(assertion_ne) {
    int a = 42;
    int b = 99;
    TEST_ASSERT_NE(a, b, "Integers should not be equal");
    TEST_ASSERT_NE(1, 2, "Different values");
}

// Test 4: String operations
TEST(string_operations) {
    const char* str = "test";
    TEST_ASSERT(str != nullptr, "String should not be null");
    TEST_ASSERT(std::strlen(str) == 4, "String length");
    TEST_ASSERT(str[0] == 't', "First character");
}

// Test 5: Multiple assertions in one test
TEST(multiple_assertions) {
    TEST_ASSERT(1 > 0, "1 is greater than 0");
    TEST_ASSERT(10 < 20, "10 is less than 20");
    TEST_ASSERT(-5 < 0, "Negative numbers");
    
    int x = 5;
    TEST_ASSERT(x >= 5, "Greater than or equal");
    TEST_ASSERT(x <= 5, "Less than or equal");
}

// Test 6: Test with loop
TEST(loop_test) {
    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT(i >= 0, "Loop counter should be non-negative");
        TEST_ASSERT(i < 10, "Loop counter should be less than 10");
    }
}

// Test 7: Test with conditional logic
TEST(conditional_logic) {
    int value = 42;
    
    if (value > 0) {
        TEST_ASSERT(value > 0, "Value is positive");
    }
    
    if (value == 42) {
        TEST_ASSERT_EQ(value, 42, "Value is 42");
    }
    else {
        TEST_ASSERT(false, "Should not reach here");
    }
}

// Test 8: Array operations
TEST(array_operations) {
    int arr[] = {1, 2, 3, 4, 5};
    int size = sizeof(arr) / sizeof(arr[0]);
    
    TEST_ASSERT_EQ(size, 5, "Array size");
    TEST_ASSERT_EQ(arr[0], 1, "First element");
    TEST_ASSERT_EQ(arr[4], 5, "Last element");
    
    int sum = 0;
    for (int i = 0; i < size; ++i) {
        sum += arr[i];
    }
    TEST_ASSERT_EQ(sum, 15, "Sum of elements");
}

// Test 9: Pointer operations
TEST(pointer_operations) {
    int value = 100;
    int* ptr = &value;
    
    TEST_ASSERT(ptr != nullptr, "Pointer should not be null");
    TEST_ASSERT_EQ(*ptr, 100, "Dereferenced value");
    
    *ptr = 200;
    TEST_ASSERT_EQ(value, 200, "Modified value");
}

// Test 10: Boolean logic
TEST(boolean_logic) {
    bool a = true;
    bool b = false;
    
    TEST_ASSERT(a, "a is true");
    TEST_ASSERT(!b, "b is false");
    TEST_ASSERT(a && !b, "AND logic");
    TEST_ASSERT(a || b, "OR logic");
    TEST_ASSERT_NE(a, b, "Different boolean values");
}

/**
 * @brief Main function - runs all tests.
 */
int main(int argc, char** argv) {
    return run_tests(argc, argv);
}

