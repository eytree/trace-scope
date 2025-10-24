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
    TRC_SCOPE();
    TRC_MSG("Calculating factorial of %d", n);
    
    if (n < 0) {
        TRC_MSG("Error: factorial of negative number %d", n);
        return -1;
    }
    
    if (n == 0 || n == 1) {
        TRC_MSG("Factorial of %d is 1", n);
        return 1;
    }
    
    long long result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
        if (i % 10 == 0) {  // Log progress every 10 iterations
            TRC_MSG("Factorial progress: %d! = %lld", i, result);
        }
    }
    
    TRC_MSG("Factorial of %d is %lld", n, result);
    return result;
}

/**
 * @brief Calculate the nth Fibonacci number
 */
long long fibonacci(int n) {
    TRC_SCOPE();
    TRC_MSG("Calculating Fibonacci number at position %d", n);
    
    if (n < 0) {
        TRC_MSG("Error: Fibonacci position cannot be negative: %d", n);
        return -1;
    }
    
    if (n == 0) {
        TRC_MSG("Fibonacci(0) = 0");
        return 0;
    }
    
    if (n == 1) {
        TRC_MSG("Fibonacci(1) = 1");
        return 1;
    }
    
    long long a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        long long temp = a + b;
        a = b;
        b = temp;
        
        if (i % 5 == 0) {  // Log progress every 5 iterations
            TRC_MSG("Fibonacci progress: F(%d) = %lld", i, b);
        }
    }
    
    TRC_MSG("Fibonacci(%d) = %lld", n, b);
    return b;
}

/**
 * @brief Calculate the greatest common divisor of two numbers
 */
int gcd(int a, int b) {
    TRC_SCOPE();
    TRC_MSG("Calculating GCD of %d and %d", a, b);
    
    // Handle negative numbers
    a = (a < 0) ? -a : a;
    b = (b < 0) ? -b : b;
    
    TRC_MSG("Using absolute values: %d and %d", a, b);
    
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
        TRC_MSG("GCD step: a=%d, b=%d", a, b);
    }
    
    TRC_MSG("GCD of %d and %d is %d", a, b, a);
    return a;
}

/**
 * @brief Check if a number is prime
 */
bool is_prime(int n) {
    TRC_SCOPE();
    TRC_MSG("Checking if %d is prime", n);
    
    if (n < 2) {
        TRC_MSG("%d is not prime (less than 2)", n);
        return false;
    }
    
    if (n == 2) {
        TRC_MSG("2 is prime");
        return true;
    }
    
    if (n % 2 == 0) {
        TRC_MSG("%d is not prime (even number)", n);
        return false;
    }
    
    int limit = static_cast<int>(std::sqrt(n));
    TRC_MSG("Checking divisibility up to %d", limit);
    
    for (int i = 3; i <= limit; i += 2) {
        if (n % i == 0) {
            TRC_MSG("%d is not prime (divisible by %d)", n, i);
            return false;
        }
    }
    
    TRC_MSG("%d is prime", n);
    return true;
}

/**
 * @brief Perform a complex calculation that calls multiple other functions
 */
long long complex_calculation(int n) {
    TRC_SCOPE();
    TRC_MSG("Starting complex calculation with n=%d", n);
    
    // Step 1: Calculate factorial
    TRC_MSG("Step 1: Calculating factorial");
    long long fact = factorial(n);
    
    // Step 2: Calculate Fibonacci
    TRC_MSG("Step 2: Calculating Fibonacci");
    long long fib = fibonacci(n);
    
    // Step 3: Check if n is prime
    TRC_MSG("Step 3: Checking if n is prime");
    bool prime = is_prime(n);
    
    // Step 4: Calculate GCD of factorial and Fibonacci
    TRC_MSG("Step 4: Calculating GCD");
    int gcd_result = gcd(static_cast<int>(fact % 1000000), static_cast<int>(fib % 1000000));
    
    // Step 5: Combine results
    long long result = fact + fib + (prime ? 1 : 0) + gcd_result;
    
    TRC_MSG("Complex calculation result: %lld", result);
    TRC_MSG("  - Factorial: %lld", fact);
    TRC_MSG("  - Fibonacci: %lld", fib);
    TRC_MSG("  - Is prime: %s", prime ? "yes" : "no");
    TRC_MSG("  - GCD: %d", gcd_result);
    
    return result;
}
