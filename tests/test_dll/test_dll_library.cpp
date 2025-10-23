/**
 * @file test_dll_library.cpp
 * @brief Test DLL implementation with traced functions
 * 
 * This file implements a test DLL that contains traced functions.
 * The main executable will call these functions to verify that
 * trace state is properly shared across DLL boundaries.
 */

#include "test_dll_library.h"
#include <trace-scope/trace_scope.hpp>

/**
 * @brief Level 1 function in DLL - simple traced function
 */
void dll_function_level1() {
    TRACE_SCOPE();
    TRACE_MSG("DLL Level 1 function called");
    
    // Simulate some work
    volatile int sum = 0;
    for (int i = 0; i < 1000; ++i) {
        sum += i;
    }
    
    TRACE_MSG("DLL Level 1 completed work, sum = %d", sum);
}

/**
 * @brief Level 2 function in DLL - calls level 1
 */
void dll_function_level2() {
    TRACE_SCOPE();
    TRACE_MSG("DLL Level 2 function called");
    
    // Call level 1 function
    dll_function_level1();
    
    TRACE_MSG("DLL Level 2 completed");
}

/**
 * @brief Simple math function - addition
 */
int dll_math_add(int a, int b) {
    TRACE_SCOPE();
    TRACE_MSG("DLL Math: Adding %d + %d", a, b);
    
    int result = a + b;
    TRACE_MSG("DLL Math: Result = %d", result);
    
    return result;
}

/**
 * @brief Simple math function - multiplication
 */
int dll_math_multiply(int a, int b) {
    TRACE_SCOPE();
    TRACE_MSG("DLL Math: Multiplying %d * %d", a, b);
    
    int result = a * b;
    TRACE_MSG("DLL Math: Result = %d", result);
    
    return result;
}

/**
 * @brief Function that makes nested calls to test call stack depth
 */
void dll_nested_calls() {
    TRACE_SCOPE();
    TRACE_MSG("DLL Nested calls starting");
    
    // Call level 2 (which calls level 1)
    dll_function_level2();
    
    // Do some math
    int result1 = dll_math_add(10, 20);
    int result2 = dll_math_multiply(5, 6);
    
    TRACE_MSG("DLL Nested calls completed, results: %d, %d", result1, result2);
}
