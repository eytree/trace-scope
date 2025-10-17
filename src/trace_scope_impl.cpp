/**
 * @file trace_scope_impl.cpp
 * @brief Implementation file for DLL-safe trace_scope usage.
 *
 * This file provides a single definition point for trace_scope's global
 * state when sharing across DLL boundaries. By default, trace_scope is
 * header-only and each DLL gets its own copy of state. Use this file
 * when you need to share trace state across multiple DLLs/executables.
 *
 * ## Usage Instructions
 *
 * ### Step 1: Include this file in ONE compilation unit
 * Include this .cpp file in your main executable or one primary DLL.
 * Do NOT include it in multiple DLLs - only once per process.
 *
 * ### Step 2: Define TRACE_SCOPE_SHARED
 * When compiling ALL files that use trace_scope (including this one),
 * define TRACE_SCOPE_SHARED to enable DLL-safe mode.
 *
 * ### Step 3: Use trace_scope normally
 * In all other files, just #include <trace_scope.hpp> as usual.
 * The shared state will be used automatically.
 *
 * ## Build Examples
 *
 * ### Windows (MSVC) - Main executable with DLL
 * @code{.sh}
 * # Compile main executable
 * cl /DTRACE_SCOPE_SHARED /DTRACE_SCOPE_IMPLEMENTATION ^
 *    src/trace_scope_impl.cpp your_main.cpp /Fe:app.exe
 *
 * # Compile DLL (no TRACE_SCOPE_IMPLEMENTATION)
 * cl /DTRACE_SCOPE_SHARED your_dll.cpp /LD /Fe:your.dll
 * @endcode
 *
 * ### Linux/Mac (GCC/Clang) - Shared library
 * @code{.sh}
 * # Compile shared library
 * g++ -DTRACE_SCOPE_SHARED -DTRACE_SCOPE_IMPLEMENTATION -fPIC -shared ^
 *     src/trace_scope_impl.cpp -o libtrace.so
 *
 * # Compile application
 * g++ -DTRACE_SCOPE_SHARED your_main.cpp -L. -ltrace -o app
 * @endcode
 *
 * ## Notes
 * - Only define TRACE_SCOPE_IMPLEMENTATION in THIS file
 * - Define TRACE_SCOPE_SHARED everywhere trace_scope is used
 * - The config object and registry are exported/imported automatically
 */

#define TRACE_SCOPE_IMPLEMENTATION
#include "../include/trace_scope.hpp"

// When TRACE_SCOPE_SHARED is defined, this file provides the single
// definition of the shared config variable that will be exported
// and shared across all DLLs/executables in the process.
#if defined(TRACE_SCOPE_SHARED) && defined(TRACE_SCOPE_IMPLEMENTATION)
trace::Config trace::config;
#endif

