/**
 * @file my_library.cpp
 * @brief Example DLL implementation with traced math functions
 * 
 * This file implements a math library DLL that demonstrates
 * how to use trace-scope across DLL boundaries. All functions
 * are traced and will share state with the main executable.
 */

#include "my_library.h"
#include <trace-scope/trace_scope.hpp>
#include <cmath>

/**
 * @brief Calculate the factorial of a number
 */
long long factorial(int n) {
    TRACE_SCOPE();
    TRACE_MSG("Calculating factorial of %d", n);
    
    if (n < 0) {
        TRACE_MSG("Error: factorial of negative number %d", n);
        return -1;
    }
    
    if (n == 0 || n == 1) {
        TRACE_MSG("Factorial of %d is 1", n);
        return 1;
    }
    
    long long result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
        if (i % 10 == 0) {  // Log progress every 10 iterations
            TRACE_MSG("Factorial progress: %d! = %lld", i, result);
        }
    }
    
    TRACE_MSG("Factorial of %d is %lld", n, result);
    return result;
}

/**
 * @brief Calculate the nth Fibonacci number
 */
long long fibonacci(int n) {
    TRACE_SCOPE();
    TRACE_MSG("Calculating Fibonacci number at position %d", n);
    
    if (n < 0) {
        TRACE_MSG("Error: Fibonacci position cannot be negative: %d", n);
        return -1;
    }
    
    if (n == 0) {
        TRACE_MSG("Fibonacci(0) = 0");
        return 0;
    }
    
    if (n == 1) {
        TRACE_MSG("Fibonacci(1) = 1");
        return 1;
    }
    
    long long a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        long long temp = a + b;
        a = b;
        b = temp;
        
        if (i % 5 == 0) {  // Log progress every 5 iterations
            TRACE_MSG("Fibonacci progress: F(%d) = %lld", i, b);
        }
    }
    
    TRACE_MSG("Fibonacci(%d) = %lld", n, b);
    return b;
}

/**
 * @brief Calculate the greatest common divisor of two numbers
 */
int gcd(int a, int b) {
    TRACE_SCOPE();
    TRACE_MSG("Calculating GCD of %d and %d", a, b);
    
    // Handle negative numbers
    a = (a < 0) ? -a : a;
    b = (b < 0) ? -b : b;
    
    TRACE_MSG("Using absolute values: %d and %d", a, b);
    
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
        TRACE_MSG("GCD step: a=%d, b=%d", a, b);
    }
    
    TRACE_MSG("GCD of %d and %d is %d", a, b, a);
    return a;
}

/**
 * @brief Check if a number is prime
 */
bool is_prime(int n) {
    TRACE_SCOPE();
    TRACE_MSG("Checking if %d is prime", n);
    
    if (n < 2) {
        TRACE_MSG("%d is not prime (less than 2)", n);
        return false;
    }
    
    if (n == 2) {
        TRACE_MSG("2 is prime");
        return true;
    }
    
    if (n % 2 == 0) {
        TRACE_MSG("%d is not prime (even number)", n);
        return false;
    }
    
    int limit = static_cast<int>(std::sqrt(n));
    TRACE_MSG("Checking divisibility up to %d", limit);
    
    for (int i = 3; i <= limit; i += 2) {
        if (n % i == 0) {
            TRACE_MSG("%d is not prime (divisible by %d)", n, i);
            return false;
        }
    }
    
    TRACE_MSG("%d is prime", n);
    return true;
}

/**
 * @brief Perform a complex calculation that calls multiple other functions
 */
long long complex_calculation(int n) {
    TRACE_SCOPE();
    TRACE_MSG("Starting complex calculation with n=%d", n);
    
    // Step 1: Calculate factorial
    TRACE_MSG("Step 1: Calculating factorial");
    long long fact = factorial(n);
    
    // Step 2: Calculate Fibonacci
    TRACE_MSG("Step 2: Calculating Fibonacci");
    long long fib = fibonacci(n);
    
    // Step 3: Check if n is prime
    TRACE_MSG("Step 3: Checking if n is prime");
    bool prime = is_prime(n);
    
    // Step 4: Calculate GCD of factorial and Fibonacci
    TRACE_MSG("Step 4: Calculating GCD");
    int gcd_result = gcd(static_cast<int>(fact % 1000000), static_cast<int>(fib % 1000000));
    
    // Step 5: Combine results
    long long result = fact + fib + (prime ? 1 : 0) + gcd_result;
    
    TRACE_MSG("Complex calculation result: %lld", result);
    TRACE_MSG("  - Factorial: %lld", fact);
    TRACE_MSG("  - Fibonacci: %lld", fib);
    TRACE_MSG("  - Is prime: %s", prime ? "yes" : "no");
    TRACE_MSG("  - GCD: %d", gcd_result);
    
    return result;
}
