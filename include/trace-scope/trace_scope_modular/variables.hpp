#ifndef TRACE_SCOPE_VARIABLES_HPP
#define TRACE_SCOPE_VARIABLES_HPP

/**
 * @file variables.hpp
 * @brief Global variable declarations and instantiations
 */

#include <mutex>
#include <atomic>

namespace trace {

// Forward declarations
struct Config;
struct Registry;
struct AsyncQueue;

/**
 * @brief Global configuration instance.
 * 
 * Default configuration used when no external state is set.
 * Can be overridden by set_external_state() for DLL shared mode.
 */
inline Config config;

// Registry and async_queue are defined in functions.hpp

/**
 * @brief Statistics registration flag.
 * 
 * Tracks whether atexit handler has been registered for
 * automatic statistics printing on program exit.
 */
static bool stats_registered = false;

} // namespace trace

#endif // TRACE_SCOPE_VARIABLES_HPP



