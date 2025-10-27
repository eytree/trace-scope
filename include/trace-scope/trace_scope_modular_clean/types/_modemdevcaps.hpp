#ifndef _MODEMDEVCAPS_HPP
#define _MODEMDEVCAPS_HPP

/**
 * @file _modemdevcaps.hpp
 * @brief _MODEMDEVCAPS struct definition
 */

oundary Support:
 *   By default, this is a header-only library. Each DLL/executable gets its own
 *   copy of the trace state, which may not be desired.
 *
 *   RECOMMENDED: Simple one-line setup with TRC_SETUP_DLL_SHARED() macro:
 *
 *   In your main() function:
 *   @code
 *   #include <trace-scope/trace_scope.hpp>
 *   
 *   int main() {
 *       TRC_SETUP_DLL_SHARED();  // One line - automatic setup & cleanup!
 *       trace::get_config().out = std::fopen("trace.log", "w");
 *       // ... your code, including DLL calls
 *       return 0;  // Automatic cleanup via RAII
 *   }
 *   @endcode
 *
 *   This automatically shares Config, Registry, and Ring buffers across all DLLs.
 *   No special build flags or compilation setup required.
 *
 * Configuration Options:
 *   The library supports extensive configuration via config files or programmatic API:
 *
 *   Flush Behavior (flush_mode):
 *   - "never": No automatic flushing on scope exit
 *   - "outermost": Flush only when returning to depth 0 (default)
 

#endif // _MODEMDEVCAPS_HPP
