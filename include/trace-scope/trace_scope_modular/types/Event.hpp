#ifndef TRACE_SCOPE_EVENT_HPP
#define TRACE_SCOPE_EVENT_HPP

/**
 * @file Event.hpp
 * @brief Event struct definition for trace events
 */

#include <cstdint>

namespace trace {

// Forward declarations
enum class EventType : uint8_t;

/**
 * @brief A single trace event stored in the ring buffer.
 * 
 * Events are created for function entry/exit (via TRC_SCOPE) and
 * for log messages (via TRC_MSG).
 */
struct Event {
    uint64_t    ts_ns;                  ///< Timestamp in nanoseconds (system clock, wall-clock time)
    const char* func;                   ///< Function name (for enter/exit; null for msg)
    const char* file;                   ///< Source file path
    int         line;                   ///< Source line number
    int         depth;                  ///< Call stack depth (for indentation)
    uint32_t    tid;                    ///< Thread ID (hashed to 32-bit for display)
    uint8_t     color_offset;           ///< Thread color offset for colorize_depth mode
    EventType   type;                   ///< Event type (Enter/Exit/Msg)
    uint64_t    dur_ns;                 ///< Duration in nanoseconds (Exit only; 0 otherwise)
    char        msg[TRC_MSG_CAP + 1]; ///< Message text (Msg events only; empty otherwise)
    uint64_t    memory_rss = 0;          ///< RSS memory usage in bytes (when track_memory enabled)
};

} // namespace trace

#endif // TRACE_SCOPE_EVENT_HPP
