#ifndef TRACE_SCOPE_ENUMS_HPP
#define TRACE_SCOPE_ENUMS_HPP

/**
 * @file enums.hpp
 * @brief All enum class definitions for trace-scope
 */

namespace trace {

/**
 * @brief Tracing output mode.
 * 
 * Determines how trace events are captured and output:
 * - Buffered: Events stored in ring buffer, flushed manually (default, best performance)
 * - Immediate: Events printed immediately, no buffering (real-time, higher overhead)
 * - Hybrid: Events both buffered AND printed immediately (best of both worlds)
 */
enum class TracingMode {
    Buffered,   ///< Default: events buffered in ring buffer, manual flush required
    Immediate,  ///< Real-time output: bypass ring buffer, print immediately
    Hybrid      ///< Hybrid: buffer events AND print immediately for real-time + history
};

/**
 * @brief Flush behavior modes for scope exit.
 */
enum class FlushMode {
    NEVER,           ///< No auto-flush on scope exit
    OUTERMOST_ONLY,  ///< Flush only when depth returns to 0 (default)
    EVERY_SCOPE      ///< Flush on every scope exit (high overhead)
};

/**
 * @brief Shared memory usage modes.
 */
enum class SharedMemoryMode {
    AUTO,      ///< Auto-detect: use shared if DLL detected (default)
    DISABLED,  ///< Never use shared memory (force thread_local)
    ENABLED    ///< Always use shared memory
};

/**
 * @brief Output directory layout options
 */
enum class OutputLayout {
    Flat,      ///< All files in output_dir: output_dir/trace_*.trc
    ByDate,    ///< Organized by date: output_dir/2025-10-20/trace_*.trc
    BySession  ///< Organized by session: output_dir/session_001/trace_*.trc
};

/**
 * @brief Type of trace event
 */
enum class EventType : uint8_t { 
    Enter = 0,  ///< Function entry
    Exit = 1,   ///< Function exit
    Msg = 2     ///< Message/log event
};

} // namespace trace

#endif // TRACE_SCOPE_ENUMS_HPP



