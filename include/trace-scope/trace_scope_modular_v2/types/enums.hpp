#ifndef ENUMS_HPP
#define ENUMS_HPP

/**
 * @file enums.hpp
 * @brief Enum definitions
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

enum class TracingMode {
    Buffered,   ///< Default: events buffered in ring buffer, manual flush required
    Immediate,  ///< Real-time output: bypass ring buffer, print immediately
    Hybrid      ///< Hybrid: buffer events AND print immediately for real-time + history
}

enum class FlushMode {
    NEVER,           ///< No auto-flush on scope exit
    OUTERMOST_ONLY,  ///< Flush only when depth returns to 0 (default)
    EVERY_SCOPE      ///< Flush on every scope exit (high overhead)
}

enum class SharedMemoryMode {
    AUTO,      ///< Auto-detect: use shared if DLL detected (default)
    DISABLED,  ///< Never use shared memory (force thread_local)
    ENABLED    ///< Always use shared memory
}

enum class EventType : uint8_t { 
    Enter = 0,  ///< Function entry
    Exit = 1,   ///< Function exit
    Msg = 2     ///< Message/log event
}

} // namespace trace


#endif // ENUMS_HPP
