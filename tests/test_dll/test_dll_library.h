#pragma once

#ifdef _WIN32
  #ifdef TEST_DLL_EXPORTS
    #define TEST_DLL_API __declspec(dllexport)
  #else
    #define TEST_DLL_API __declspec(dllimport)
  #endif
#else
  #define TEST_DLL_API
#endif

/**
 * @brief Test functions that will be traced from within the DLL
 */

TEST_DLL_API void dll_function_level1();
TEST_DLL_API void dll_function_level2();
TEST_DLL_API int dll_math_add(int a, int b);
TEST_DLL_API int dll_math_multiply(int a, int b);
TEST_DLL_API void dll_nested_calls();
