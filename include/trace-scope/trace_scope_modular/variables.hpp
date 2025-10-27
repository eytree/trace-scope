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

/**
 * @brief Global registry instance.
 * 
 * Manages all thread-local ring buffers for flush operations.
 * In DLL shared mode, this is replaced by the shared registry.
 */
inline Registry& registry() {
    static Registry r;
    return r;
}

/**
 * @brief Global async queue instance.
 * 
 * Used for immediate and hybrid tracing modes to provide
 * non-blocking event output via background writer thread.
 */
inline AsyncQueue& async_queue() {
    static AsyncQueue q;
    return q;
}

/**
 * @brief Statistics registration flag.
 * 
 * Tracks whether atexit handler has been registered for
 * automatic statistics printing on program exit.
 */
static bool stats_registered = false;

} // namespace trace

#endif // TRACE_SCOPE_VARIABLES_HPP



