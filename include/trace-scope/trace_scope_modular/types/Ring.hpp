#ifndef TRACE_SCOPE_RING_HPP
#define TRACE_SCOPE_RING_HPP

/**
 * @file Ring.hpp
 * @brief Ring struct definition for per-thread ring buffer
 */

#include <cstdint>
#include <cstdio>  // For FILE*
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdarg>

namespace trace {

// Forward declarations
struct Event;
enum class EventType : uint8_t;
enum class TracingMode;
struct Config;
struct FunctionStats;
struct AsyncQueue;

// Forward declarations for functions
inline Config& get_config();
inline void print_event(const Event& e, FILE* out);
inline void flush_current_thread();
inline AsyncQueue& async_queue();
inline bool should_trace(const char* func, const char* file, int depth);
inline uint64_t get_current_rss();

/**
 * @brief Per-thread ring buffer for trace events.
 * 
 * Each thread gets its own Ring (thread_local storage). Events are written
 * lock-free to the ring buffer. When the buffer fills, oldest events are
 * overwritten (wraps counter increments).
 * 
 * Supports optional double-buffering mode (see Config::use_double_buffering):
 * - Single buffer: Events written directly to buf[0], flushed in-place (default)
 * - Double buffer: Two buffers alternate; write to one while flushing the other
 */
struct Ring {
    Event       buf[TRC_NUM_BUFFERS][TRC_RING_CAP];  ///< Circular buffer(s): [0] for single mode, [0]/[1] for double mode
    uint32_t    head[TRC_NUM_BUFFERS] = {0};            ///< Next write position per buffer
    uint64_t    wraps[TRC_NUM_BUFFERS] = {0};           ///< Number of buffer wraparounds per buffer
#if TRC_DOUBLE_BUFFER
    std::atomic<int> active_buf{0};                 ///< Active buffer index for double-buffering (0 or 1)
    std::mutex  flush_mtx;                          ///< Protects buffer swap during flush (double-buffer mode only)
#endif
    int         depth = 0;                          ///< Current call stack depth
    uint32_t    tid   = 0;                          ///< Thread ID (cached)
    uint8_t     color_offset = 0;                   ///< Thread-specific color offset (0-7) for visual distinction
    bool        registered = false;                 ///< Whether this ring is registered globally
    uint64_t    start_stack[TRC_DEPTH_MAX];       ///< Start timestamp per depth (for duration calculation)
    const char* func_stack[TRC_DEPTH_MAX];        ///< Function name per depth (for message context)
    
    /**
     * @brief Constructor: Initialize thread-specific values.
     * Defined after thread_id_hash() declaration.
     */
    Ring();
    
    /**
     * @brief Destructor: Unregister from global registry.
     * 
     * When a thread exits, its thread_local Ring is destroyed. We remove it
     * from the global registry to prevent flush_all() from accessing freed memory.
     * 
     * Note: Any unflushed events in this ring will be lost. Applications should
     * call flush_all() or enable auto_flush_at_exit before threads terminate.
     */
    inline ~Ring();  // Defined after registry() declaration

    /**
     * @brief Check if ring buffer should be auto-flushed (hybrid mode).
     * 
     * Returns true if hybrid mode is enabled and buffer usage exceeds
     * the configured threshold.
     * 
     * @return true if buffer should be flushed
     */
    inline bool should_auto_flush() const;

    /**
     * @brief Write a trace event (Enter/Exit/Msg).
     * 
     * In immediate mode, prints directly. In buffered mode, writes to ring buffer.
     * Maintains call stack depth and tracks start times for duration calculation.
     * 
     * @param type Event type (Enter/Exit/Msg)
     * @param func Function name (null for Msg)
     * @param file Source file path
     * @param line Source line number
     */
    inline void write(EventType type, const char* func, const char* file, int line);

    /**
     * @brief Write a formatted message event.
     * 
     * Retrieves current function context from the function stack and
     * formats the message using vsnprintf. In immediate mode, prints directly.
     * In buffered mode, writes to ring buffer.
     * 
     * @param file Source file path
     * @param line Source line number
     * @param fmt Printf-style format string
     * @param ap Variable argument list
     */
    inline void write_msg(const char* file, int line, const char* fmt, va_list ap);
};

} // namespace trace

#endif // TRACE_SCOPE_RING_HPP
