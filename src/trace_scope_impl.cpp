/**
 * @file trace_scope_impl.cpp
 * @brief Implementation file for DLL-safe trace_scope usage.
 *
 * To use trace_scope across DLL boundaries:
 * 1. Include this file in your main executable or one shared DLL
 * 2. Define TRACE_SCOPE_SHARED when compiling this and all other files
 * 3. In other files, just #include <trace_scope.hpp> normally
 *
 * Build example (main executable):
 *   cl /DTRACE_SCOPE_SHARED /DTRACE_SCOPE_IMPLEMENTATION trace_scope_impl.cpp your_main.cpp /link /DLL
 *
 * Build example (other DLLs):
 *   cl /DTRACE_SCOPE_SHARED other_dll.cpp /link /DLL
 */

#define TRACE_SCOPE_IMPLEMENTATION
#include "../include/trace_scope.hpp"

// When TRACE_SCOPE_SHARED is defined, this file provides the single
// definition of the shared config variable
#if defined(TRACE_SCOPE_SHARED) && defined(TRACE_SCOPE_IMPLEMENTATION)
trace::Config trace::config;
#endif

