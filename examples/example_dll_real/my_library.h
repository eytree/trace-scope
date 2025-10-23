#pragma once

#ifdef _WIN32
  #ifdef MY_LIBRARY_EXPORTS
    #define MY_LIBRARY_API __declspec(dllexport)
  #else
    #define MY_LIBRARY_API __declspec(dllimport)
  #endif
#else
  #define MY_LIBRARY_API
#endif

/**
 * @brief Simple math library with traced functions
 * 
 * This is an example of a DLL that uses trace-scope for function tracing.
 * The main executable will set up shared trace state, and all functions
 * in this DLL will be traced with the same state.
 */

/**
 * @brief Calculate the factorial of a number
 * @param n The number to calculate factorial for
 * @return The factorial of n
 */
MY_LIBRARY_API long long factorial(int n);

/**
 * @brief Calculate the nth Fibonacci number
 * @param n The position in the Fibonacci sequence
 * @return The nth Fibonacci number
 */
MY_LIBRARY_API long long fibonacci(int n);

/**
 * @brief Calculate the greatest common divisor of two numbers
 * @param a First number
 * @param b Second number
 * @return The GCD of a and b
 */
MY_LIBRARY_API int gcd(int a, int b);

/**
 * @brief Check if a number is prime
 * @param n The number to check
 * @return true if n is prime, false otherwise
 */
MY_LIBRARY_API bool is_prime(int n);

/**
 * @brief Perform a complex calculation that calls multiple other functions
 * @param n Input value for the calculation
 * @return Result of the complex calculation
 */
MY_LIBRARY_API long long complex_calculation(int n);
