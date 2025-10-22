#pragma once
/**
 * @file trace_scope.hpp
 * @brief Header-only function-scope tracing with per-thread ring buffers.
 *
 * Features:
 *  - TRACE_SCOPE(): records enter/exit with depth indentation and duration.
 *  - TRACE_MSG(fmt, ...): buffered message event at current depth (file:line).
 *  - Per-thread lock-free ring buffer; global flush to text.
 *  - dump_binary(path): compact binary dump + tools/trc_pretty.py pretty printer.
 *  - Optional DLL-safe mode via TRACE_SCOPE_IMPLEMENTATION.
 *
 * Build-time defines:
 *   TRACE_ENABLED   (default 1)
 *   TRACE_RING_CAP  (default 4096)  // events per thread
 *   TRACE_MSG_CAP   (default 192)   // max message size
 *   TRACE_DEPTH_MAX (default 512)   // max nesting depth tracked for durations
 *
 * DLL Boundary Support:
 *   By default, this is a header-only library. Each DLL/executable gets its own
 *   copy of the trace state, which may not be desired.
 *
 *   To share trace state across DLL boundaries:
 *   1. In ONE .cpp file in your main executable or shared DLL, define:
 *      #define TRACE_SCOPE_IMPLEMENTATION
 *      #include <trace_scope.hpp>
 *   2. In all other files, just #include <trace_scope.hpp> normally.
 *   3. Define TRACE_SCOPE_SHARED when building DLLs that need to share state.
 *
 *   This creates exported symbols that can be shared across DLL boundaries.
 */

#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <thread>
#include <algorithm>
#include <sstream>
#include <map>
#include <string>
#include <vector>
#include <atomic>
#include <filesystem>

// Platform-specific includes for memory sampling
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
// Undefine Windows macros that conflict with std::min/max
#undef min
#undef max
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#endif

// DLL export/import macros for shared state across DLL boundaries
#ifndef TRACE_SCOPE_API
  #if defined(TRACE_SCOPE_SHARED)
    #if defined(_WIN32) || defined(__CYGWIN__)
      #if defined(TRACE_SCOPE_IMPLEMENTATION)
        #define TRACE_SCOPE_API __declspec(dllexport)
      #else
        #define TRACE_SCOPE_API __declspec(dllimport)
      #endif
    #elif defined(__GNUC__) && __GNUC__ >= 4
      #define TRACE_SCOPE_API __attribute__((visibility("default")))
    #else
      #define TRACE_SCOPE_API
    #endif
  #else
    #define TRACE_SCOPE_API
  #endif
#endif

// Storage class for variables: inline for header-only, extern for DLL shared
#if defined(TRACE_SCOPE_SHARED) && !defined(TRACE_SCOPE_IMPLEMENTATION)
  #define TRACE_SCOPE_VAR extern TRACE_SCOPE_API
#else
  #define TRACE_SCOPE_VAR inline
#endif

#ifndef TRACE_ENABLED
#define TRACE_ENABLED 1
#endif

#ifndef TRACE_RING_CAP
#define TRACE_RING_CAP 4096
#endif

#ifndef TRACE_MSG_CAP
#define TRACE_MSG_CAP 192
#endif

#ifndef TRACE_DEPTH_MAX
#define TRACE_DEPTH_MAX 512
#endif

#ifndef TRACE_DOUBLE_BUFFER
#define TRACE_DOUBLE_BUFFER 0  // Default: disabled to save memory (~1.2MB per thread)
#endif

#if TRACE_DOUBLE_BUFFER
#define TRACE_NUM_BUFFERS 2
#else
#define TRACE_NUM_BUFFERS 1
#endif

// Version information - keep in sync with VERSION file at project root
#define TRACE_SCOPE_VERSION "0.9.0-alpha"
#define TRACE_SCOPE_VERSION_MAJOR 0
#define TRACE_SCOPE_VERSION_MINOR 9
#define TRACE_SCOPE_VERSION_PATCH 0

namespace trace {

// Forward declarations
struct Config;
struct AsyncQueue;
inline AsyncQueue& async_queue();

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
 * @brief INI file parser utilities for configuration loading.
 * 
 * Simple, dependency-free INI parser supporting:
 * - Comments (# and ;)
 * - Sections [section_name]
 * - Key-value pairs (key = value)
 * - Boolean, integer, float, and string values
 * - Quoted and unquoted strings
 */
namespace ini_parser {

/**
 * @brief Trim whitespace from both ends of a string.
 */
inline std::string trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.length();
    
    while (start < end && std::isspace((unsigned char)str[start])) ++start;
    while (end > start && std::isspace((unsigned char)str[end - 1])) --end;
    
    return str.substr(start, end - start);
}

/**
 * @brief Parse boolean value from string.
 * 
 * Accepts: true/false, 1/0, on/off, yes/no (case-insensitive)
 */
inline bool parse_bool(const std::string& value) {
    std::string v = trim(value);
    
    // Convert to lowercase for case-insensitive comparison
    for (char& c : v) {
        c = (char)std::tolower((unsigned char)c);
    }
    
    if (v == "true" || v == "1" || v == "on" || v == "yes") return true;
    if (v == "false" || v == "0" || v == "off" || v == "no") return false;
    
    return false;  // Default to false on parse error
}

/**
 * @brief Parse integer value from string.
 */
inline int parse_int(const std::string& value) {
    try {
        return std::stoi(trim(value));
    } catch (...) {
        return 0;
    }
}

/**
 * @brief Parse float value from string.
 */
inline float parse_float(const std::string& value) {
    try {
        return std::stof(trim(value));
    } catch (...) {
        return 0.0f;
    }
}

/**
 * @brief Remove quotes from string if present.
 */
inline std::string unquote(const std::string& str) {
    std::string s = trim(str);
    if (s.length() >= 2 && s[0] == '"' && s[s.length()-1] == '"') {
        return s.substr(1, s.length() - 2);
    }
    return s;
}

} // namespace ini_parser

/**
 * @brief Filtering utilities for selective tracing.
 * 
 * Provides simple wildcard pattern matching for filtering functions and files.
 */
namespace filter_utils {

/**
 * @brief Simple wildcard pattern matching (* matches zero or more characters).
 * 
 * @param pattern Pattern with * wildcards (e.g., "test_*", "*_func", "*mid*")
 * @param text Text to match against
 * @return true if text matches pattern
 */
inline bool wildcard_match(const char* pattern, const char* text) {
    if (!pattern || !text) return false;
    
    // Iterate through pattern and text
    while (*pattern && *text) {
        if (*pattern == '*') {
            // Skip consecutive wildcards
            while (*pattern == '*') ++pattern;
            
            // If wildcard is at end, match succeeds
            if (!*pattern) return true;
            
            // Try matching rest of pattern with each position in text
            while (*text) {
                if (wildcard_match(pattern, text)) return true;
                ++text;
            }
            return false;
        }
        else if (*pattern == *text) {
            ++pattern;
            ++text;
        }
        else {
            return false;
        }
    }
    
    // Handle trailing wildcards in pattern
    while (*pattern == '*') ++pattern;
    
    return (*pattern == '\0' && *text == '\0');
}

/**
 * @brief Check if string matches any pattern in list.
 * 
 * @param text Text to match against patterns
 * @param patterns List of wildcard patterns
 * @return true if text matches at least one pattern
 */
inline bool matches_any(const char* text, const std::vector<std::string>& patterns) {
    if (!text) return false;
    if (patterns.empty()) return false;
    
    for (const auto& pattern : patterns) {
        if (wildcard_match(pattern.c_str(), text)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Check if event should be traced based on current filters.
 * 
 * Filter logic:
 * 1. Check depth filter (if set)
 * 2. Check function filters (exclude wins over include)
 * 3. Check file filters (exclude wins over include)
 * 
 * @param func Function name (can be null for Msg events)
 * @param file File path
 * @param depth Current call depth
 * @return true if event should be traced, false if filtered out
 */
inline bool should_trace(const char* func, const char* file, int depth);

} // namespace filter_utils

/**
 * @brief Global configuration for trace output formatting and behavior.
 * 
 * All settings can be modified at runtime before tracing begins.
 * Config changes during tracing are not thread-safe.
 */
struct Config {
    FILE* out = stdout;               ///< Output file stream (default: stdout)
    bool print_timing = true;         ///< Show function durations with auto-scaled units
    bool print_timestamp = false;     ///< Show ISO timestamps [YYYY-MM-DD HH:MM:SS.mmm] (opt-in)
    bool print_thread = true;         ///< Show thread ID in hex format
    bool auto_flush_at_exit = false;  ///< Automatically flush when outermost scope exits (opt-in)
    
    // Tracing mode
    TracingMode mode = TracingMode::Buffered;  ///< Tracing output mode (default: Buffered)
    FILE* immediate_out = nullptr;    ///< Separate output stream for immediate output in Hybrid mode (nullptr = use 'out')
    float auto_flush_threshold = 0.9f; ///< Auto-flush when buffer reaches this fraction full in Hybrid mode (0.0-1.0, default 0.9 = 90%)
    
    // Async immediate mode configuration
    int immediate_flush_interval_ms = 1;  ///< Flush interval for async immediate mode (default: 1ms, 0 = flush every event)
    size_t immediate_queue_size = 128;    ///< Max queue size hint for async immediate mode (default: 128)

    // Prefix content control
    bool include_file_line = true;    ///< Include filename:line in prefix block

    // Filename rendering options
    bool include_filename = true;     ///< Show filename in prefix
    bool show_full_path = false;      ///< Show full path vs basename only
    int  filename_width = 20;         ///< Fixed width for filename column
    int  line_width     = 5;          ///< Fixed width for line number
    
    // Function name rendering options
    bool include_function_name = true;  ///< Show function name in prefix (line number pairs with this)
    int  function_width = 20;           ///< Fixed width for function name column
    
    // Indentation and marker visualization
    bool show_indent_markers = true;    ///< Show visual markers for indentation levels
    const char* indent_marker = "| ";   ///< Marker for each indentation level (e.g., "| ", "  ", "│ ")
    const char* enter_marker = "-> ";   ///< Marker for function entry (e.g., "-> ", "↘ ", "► ")
    const char* exit_marker = "<- ";    ///< Marker for function exit (e.g., "<- ", "↖ ", "◄ ")
    const char* msg_marker = "- ";      ///< Marker for message events (e.g., "- ", "• ", "* ")
    
    // ANSI color support
    bool colorize_depth = false;        ///< Colorize output based on call depth (opt-in, ANSI colors)
    
    // Double-buffering for high-frequency tracing
    bool use_double_buffering = false;  ///< Enable double-buffering (opt-in, eliminates flush race conditions)
                                        ///< Pros: Safe concurrent write/flush, zero disruption, better for high-frequency tracing
                                        ///< Cons: 2x memory per thread (~4MB default), slightly more complex
                                        ///< Use when: Generating millions of events/sec with frequent flushing
    
    // Filtering and selective tracing
    struct {
        std::vector<std::string> include_functions;  ///< Include function patterns (empty = trace all)
        std::vector<std::string> exclude_functions;  ///< Exclude function patterns (higher priority than include)
        std::vector<std::string> include_files;      ///< Include file patterns (empty = trace all)
        std::vector<std::string> exclude_files;      ///< Exclude file patterns (higher priority than include)
        int max_depth = -1;                          ///< Maximum trace depth (-1 = unlimited)
    } filter;
    
    // Performance metrics and memory tracking
    bool print_stats = false;        ///< Print performance statistics at program exit
    bool track_memory = false;       ///< Sample RSS memory at each trace point (low overhead ~1-5µs)
    
    // Binary dump configuration
    const char* dump_prefix = "trace";  ///< Filename prefix for binary dumps (default: "trace")
    const char* dump_suffix = ".trc";   ///< File extension for binary dumps (default: ".trc")
    const char* output_dir = nullptr;   ///< Output directory (nullptr = current directory)
    
    /// Output directory layout options
    enum class OutputLayout {
        Flat,      ///< All files in output_dir: output_dir/trace_*.trc
        ByDate,    ///< Organized by date: output_dir/2025-10-20/trace_*.trc
        BySession  ///< Organized by session: output_dir/session_001/trace_*.trc
    };
    OutputLayout output_layout = OutputLayout::Flat;  ///< Directory structure layout (default: Flat)
    int current_session = 0;  ///< Session number for BySession layout (0 = auto-increment)
    
    /**
     * @brief Load configuration from INI file.
     * 
     * Parses an INI file and applies settings to this Config instance.
     * Supports sections: [output], [display], [formatting], [markers], [modes], 
     *                     [filter], [performance], [dump]
     * 
     * @param path Path to INI file (relative or absolute)
     * @return true on success, false if file not found or critical error
     * 
     * Example INI format:
     * @code
     * [display]
     * print_timing = true
     * print_timestamp = false
     * 
     * [dump]
     * prefix = trace
     * suffix = .trc
     * output_dir = logs
     * layout = date
     * 
     * [markers]
     * indent_marker = | 
     * enter_marker = -> 
     * @endcode
     */
    inline bool load_from_file(const char* path);
};
TRACE_SCOPE_VAR Config config;

// Forward declarations for external state system
struct Registry;
inline Config& get_config();
inline void flush_current_thread();

// External state pointers for DLL-safe cross-boundary tracing (header-only solution)
// When set, these override the default inline instances
inline Config* g_external_config = nullptr;
inline Registry* g_external_registry = nullptr;

/** @brief Type of trace event */
enum class EventType : uint8_t { 
    Enter = 0,  ///< Function entry
    Exit = 1,   ///< Function exit
    Msg = 2     ///< Message/log event
};

/**
 * @brief A single trace event stored in the ring buffer.
 * 
 * Events are created for function entry/exit (via TRACE_SCOPE) and
 * for log messages (via TRACE_MSG).
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
    char        msg[TRACE_MSG_CAP + 1]; ///< Message text (Msg events only; empty otherwise)
    uint64_t    memory_rss = 0;          ///< RSS memory usage in bytes (when track_memory enabled)
};

// Forward declaration for immediate mode
inline void print_event(const Event& e, FILE* out);

/**
 * @brief Performance statistics for a single function.
 */
struct FunctionStats {
    const char* func_name;     ///< Function name
    uint64_t    call_count;   ///< Number of times function was called
    uint64_t    total_ns;     ///< Total execution time in nanoseconds
    uint64_t    min_ns;       ///< Minimum execution time in nanoseconds
    uint64_t    max_ns;       ///< Maximum execution time in nanoseconds
    uint64_t    memory_delta; ///< Memory delta in bytes (peak - start RSS)
    
    double avg_ns() const { return call_count > 0 ? (double)total_ns / call_count : 0.0; }
};

/**
 * @brief Per-thread performance statistics.
 */
struct ThreadStats {
    uint32_t tid;                                    ///< Thread ID
    std::vector<FunctionStats> functions;            ///< Function statistics for this thread
    uint64_t total_events;                          ///< Total events in this thread
    uint64_t peak_rss;                              ///< Peak RSS memory usage for this thread
};

/**
 * @brief Memory sampling utilities for RSS tracking.
 */
namespace memory_utils {

/**
 * @brief Get current process RSS (Resident Set Size) in bytes.
 * 
 * Cross-platform implementation:
 * - Windows: Uses GetProcessMemoryInfo()
 * - Linux: Parses /proc/self/status for VmRSS
 * - macOS: Uses task_info() with MACH_TASK_BASIC_INFO
 * 
 * @return RSS in bytes, or 0 if unable to determine
 */
inline uint64_t get_current_rss() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#elif defined(__linux__)
    FILE* f = fopen("/proc/self/status", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                uint64_t rss_kb;
                if (sscanf(line + 6, "%lu", &rss_kb) == 1) {
                    fclose(f);
                    return rss_kb * 1024;  // Convert KB to bytes
                }
            }
        }
        fclose(f);
    }
#elif defined(__APPLE__)
    struct task_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS) {
        return info.resident_size;
    }
#endif
    return 0;
}

} // namespace memory_utils

/**
 * @brief Async queue for immediate mode with background writer thread.
 * 
 * MPSC (Multi-Producer Single-Consumer) queue for non-blocking event writes.
 * Traced threads enqueue events without blocking on I/O. Background writer
 * thread drains queue and writes events to file with configurable batching.
 */
struct AsyncQueue {
    std::mutex mtx;                             ///< Protects queue access
    std::vector<Event> queue;                   ///< Event queue
    std::condition_variable cv;                 ///< Notifies writer thread of new events
    std::atomic<bool> running{false};           ///< Writer thread running flag
    std::thread writer_thread;                  ///< Background writer thread
    FILE* output_file = nullptr;                ///< Output file stream
    std::atomic<uint64_t> enqueue_count{0};     ///< Total events enqueued (for flush_now)
    std::atomic<uint64_t> write_count{0};       ///< Total events written (for flush_now)
    
    // Configuration (copied from Config on start())
    int flush_interval_ms = 1;                  ///< Flush interval in milliseconds
    size_t batch_size = 128;                    ///< Max events per batch write
    
    /**
     * @brief Constructor (does nothing - call start() to begin).
     */
    AsyncQueue() = default;
    
    /**
     * @brief Destructor: Stops writer thread and flushes remaining events.
     */
    ~AsyncQueue() {
        stop();
    }
    
    /**
     * @brief Start the async writer thread.
     * @param out Output file stream
     */
    inline void start(FILE* out) {
        if (running.load()) return;  // Already started
        
        output_file = out;
        running.store(true);
        writer_thread = std::thread([this]() { writer_loop(); });
    }
    
    /**
     * @brief Stop the writer thread and flush remaining events.
     */
    inline void stop() {
        if (!running.load()) return;  // Not running
        
        running.store(false);
        cv.notify_one();
        
        if (writer_thread.joinable()) {
            writer_thread.join();
        }
    }
    
    /**
     * @brief Enqueue an event (non-blocking, called from traced threads).
     * @param e Event to enqueue
     */
    inline void enqueue(const Event& e) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push_back(e);
        }
        enqueue_count.fetch_add(1, std::memory_order_relaxed);
        cv.notify_one();
    }
    
    /**
     * @brief Force immediate flush of queue (blocks until empty).
     * 
     * Waits up to 1 second for queue to drain. Used when synchronous
     * semantics are needed (e.g., before crash, in tests).
     */
    inline void flush_now() {
        // Wake up writer thread
        cv.notify_one();
        
        // Spin-wait until queue is drained (or timeout)
        auto start = std::chrono::steady_clock::now();
        while (enqueue_count.load(std::memory_order_relaxed) != 
               write_count.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            
            // Timeout after 1 second
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::seconds(1)) {
                std::fprintf(stderr, "trace-scope: Warning: flush_immediate_queue() timeout after 1s\n");
                break;
            }
        }
    }
    
private:
    /**
     * @brief Background writer thread loop.
     * 
     * Waits for events with timeout (flush_interval_ms), drains queue,
     * and writes all events to file in a batch.
     */
    inline void writer_loop() {
        while (running.load(std::memory_order_relaxed)) {
            std::vector<Event> local;
            
            {
                std::unique_lock<std::mutex> lock(mtx);
                
                // Wait with timeout for new events
                cv.wait_for(lock, std::chrono::milliseconds(flush_interval_ms),
                           [this]() { 
                               return !queue.empty() || !running.load(std::memory_order_relaxed); 
                           });
                
                // Swap queues (fast, O(1))
                local.swap(queue);
            }
            
            // Write all events (outside lock - no contention with enqueue)
            for (const auto& e : local) {
                if (output_file) {
                    print_event(e, output_file);
                }
            }
            
            // Flush to disk and update write counter
            if (!local.empty() && output_file) {
                std::fflush(output_file);
                write_count.fetch_add(local.size(), std::memory_order_relaxed);
            }
        }
        
        // Final flush on shutdown - ensure no events lost
        std::vector<Event> remaining;
        {
            std::lock_guard<std::mutex> lock(mtx);
            remaining.swap(queue);
        }
        
        for (const auto& e : remaining) {
            if (output_file) {
                print_event(e, output_file);
            }
        }
        
        if (!remaining.empty() && output_file) {
            std::fflush(output_file);
            write_count.fetch_add(remaining.size(), std::memory_order_relaxed);
        }
    }
};

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
    Event       buf[TRACE_NUM_BUFFERS][TRACE_RING_CAP];  ///< Circular buffer(s): [0] for single mode, [0]/[1] for double mode
    uint32_t    head[TRACE_NUM_BUFFERS] = {0};            ///< Next write position per buffer
    uint64_t    wraps[TRACE_NUM_BUFFERS] = {0};           ///< Number of buffer wraparounds per buffer
#if TRACE_DOUBLE_BUFFER
    std::atomic<int> active_buf{0};                 ///< Active buffer index for double-buffering (0 or 1)
    std::mutex  flush_mtx;                          ///< Protects buffer swap during flush (double-buffer mode only)
#endif
    int         depth = 0;                          ///< Current call stack depth
    uint32_t    tid   = 0;                          ///< Thread ID (cached)
    uint8_t     color_offset = 0;                   ///< Thread-specific color offset (0-7) for visual distinction
    bool        registered = false;                 ///< Whether this ring is registered globally
    uint64_t    start_stack[TRACE_DEPTH_MAX];       ///< Start timestamp per depth (for duration calculation)
    const char* func_stack[TRACE_DEPTH_MAX];        ///< Function name per depth (for message context)
    
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
    inline bool should_auto_flush() const {
        if (get_config().mode != TracingMode::Hybrid) {
            return false;
        }
        
        // Check active buffer usage
        int buf_idx = 0;
#if TRACE_DOUBLE_BUFFER
        if (get_config().use_double_buffering) {
            buf_idx = active_buf.load(std::memory_order_relaxed);
        }
#endif
        float usage = (float)head[buf_idx] / (float)TRACE_RING_CAP;
        if (wraps[buf_idx] > 0) {
            usage = 1.0f;  // Already wrapped = 100% full
        }
        
        return usage >= get_config().auto_flush_threshold;
    }

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
    inline void write(EventType type, const char* func, const char* file, int line) {
#if TRACE_ENABLED
        // Apply filters - skip if filtered out, but still update depth
        if (!filter_utils::should_trace(func, file, depth)) {
            // Must still track depth to maintain correct nesting
            if (type == EventType::Enter) {
                int d = depth;
                if (d < TRACE_DEPTH_MAX) {
                    const auto now = std::chrono::system_clock::now().time_since_epoch();
                    uint64_t now_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
                    start_stack[d] = now_ns;
                    func_stack[d] = func;
                }
                ++depth;
            }
            else if (type == EventType::Exit) {
                depth = std::max(0, depth - 1);
            }
            return;  // Filtered out - don't write event
        }
        
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t now_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

        Event e;
        e.ts_ns = now_ns;
        e.func  = func;
        e.file  = file;
        e.line  = line;
        e.tid   = tid;
        e.color_offset = color_offset;
        e.type  = type;
        e.msg[0]= '\0';
        e.dur_ns= 0;
        
        // Sample memory if tracking is enabled
        if (get_config().track_memory) {
            e.memory_rss = memory_utils::get_current_rss();
        } else {
            e.memory_rss = 0;
        }

        if (type == EventType::Enter) {
            int d = depth;
            e.depth = d;
            if (d < TRACE_DEPTH_MAX) {
                start_stack[d] = now_ns;
                func_stack[d] = func;  // Track function name for messages
            }
            ++depth;
        } else if (type == EventType::Exit) {
            depth = std::max(0, depth - 1);
            int d = std::max(0, depth);
            e.depth = d;
            if (d < TRACE_DEPTH_MAX) {
                uint64_t start_ns = start_stack[d];
                e.dur_ns = now_ns - start_ns;
            }
        } else {
            e.depth = depth;
        }

        // Hybrid mode: buffer AND print immediately, with auto-flush
        if (get_config().mode == TracingMode::Hybrid) {
            // Write to ring buffer first (single or double-buffer mode)
            int buf_idx = 0;
#if TRACE_DOUBLE_BUFFER
            if (get_config().use_double_buffering) {
                buf_idx = active_buf.load(std::memory_order_relaxed);
            }
#else
            if (get_config().use_double_buffering) {
                static bool warned = false;
                if (!warned) {
                    std::fprintf(stderr, "trace-scope: ERROR: use_double_buffering=true but not compiled with TRACE_DOUBLE_BUFFER=1\n");
                    std::fprintf(stderr, "trace-scope: Recompile with -DTRACE_DOUBLE_BUFFER=1 or add before include\n");
                    warned = true;
                }
            }
#endif
            buf[buf_idx][head[buf_idx]] = e;
            head[buf_idx] = (head[buf_idx] + 1) % TRACE_RING_CAP;
            if (head[buf_idx] == 0) {
                ++wraps[buf_idx];
            }
            
            // Check if we need to auto-flush BEFORE acquiring lock
            bool needs_flush = should_auto_flush();
            
            // Also print immediately for real-time visibility (using async queue)
            {
                // Ensure async queue is started (thread-safe via std::call_once)
                static std::once_flag async_init_flag;
                std::call_once(async_init_flag, []() {
                    FILE* imm_out = get_config().immediate_out;
                    if (!imm_out) {
                        imm_out = get_config().out ? get_config().out : stdout;
                    }
                    async_queue().flush_interval_ms = get_config().immediate_flush_interval_ms;
                    async_queue().batch_size = get_config().immediate_queue_size;
                    async_queue().start(imm_out);
                    
                    // Register atexit handler to stop async queue on exit
                    std::atexit([]() {
                        if (get_config().mode == TracingMode::Hybrid) {
                            async_queue().stop();  // Stops thread and flushes remaining events
                        }
                    });
                });
                
                // Enqueue event for async immediate output (non-blocking)
                async_queue().enqueue(e);
            }
            
            // Auto-flush if buffer is near capacity (outside lock to avoid deadlock)
            if (needs_flush) {
                flush_current_thread();
            }
        }
        // Immediate mode: async queue with background writer (non-blocking)
        else if (get_config().mode == TracingMode::Immediate) {
            // Ensure async queue is started (thread-safe via std::call_once)
            static std::once_flag async_init_flag;
            std::call_once(async_init_flag, []() {
                FILE* out = get_config().out ? get_config().out : stdout;
                async_queue().flush_interval_ms = get_config().immediate_flush_interval_ms;
                async_queue().batch_size = get_config().immediate_queue_size;
                async_queue().start(out);
                
                // Register atexit handler to stop async queue on exit
                std::atexit([]() {
                    if (get_config().mode == TracingMode::Immediate) {
                        async_queue().stop();  // Stops thread and flushes remaining events
                    }
                });
            });
            
            // Enqueue event (non-blocking, fast)
            async_queue().enqueue(e);
        }
        // Buffered mode: write to ring buffer only (single or double-buffer mode)
        else {
            int buf_idx = 0;
#if TRACE_DOUBLE_BUFFER
            if (get_config().use_double_buffering) {
                buf_idx = active_buf.load(std::memory_order_relaxed);
            }
#endif
            buf[buf_idx][head[buf_idx]] = e;
            head[buf_idx] = (head[buf_idx] + 1) % TRACE_RING_CAP;
            if (head[buf_idx] == 0) {
                ++wraps[buf_idx];
            }
        }
#endif
    }

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
    inline void write_msg(const char* file, int line, const char* fmt, va_list ap) {
        // Get current function name from the stack (depth is at current scope)
        const char* current_func = nullptr;
        int d = depth > 0 ? depth - 1 : 0;
        if (d < TRACE_DEPTH_MAX) {
            current_func = func_stack[d];
        }
        
        // Hybrid mode: buffer AND print immediately
        if (get_config().mode == TracingMode::Hybrid) {
            // Create event and format message
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            uint64_t now_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
            
            Event e;
            e.ts_ns = now_ns;
            e.func = current_func;
            e.file = file;
            e.line = line;
            e.tid = tid;
            e.color_offset = color_offset;
            e.type = EventType::Msg;
            e.depth = depth;
            e.dur_ns = 0;
            e.memory_rss = 0;
            
            // Format the message
            if (!fmt) {
                e.msg[0] = 0;
            }
            else {
                int n = std::vsnprintf(e.msg, TRACE_MSG_CAP, fmt, ap);
                if (n < 0) {
                    e.msg[0] = 0;
                }
                else {
                    e.msg[std::min(n, TRACE_MSG_CAP)] = 0;
                }
            }
            
            // Write to buffer
            int buf_idx = 0;
#if TRACE_DOUBLE_BUFFER
            if (get_config().use_double_buffering) {
                buf_idx = active_buf.load(std::memory_order_relaxed);
            }
#endif
            buf[buf_idx][head[buf_idx]] = e;
            head[buf_idx] = (head[buf_idx] + 1) % TRACE_RING_CAP;
            if (head[buf_idx] == 0) {
                ++wraps[buf_idx];
            }
            
            // Also enqueue to async queue for immediate output
            async_queue().enqueue(e);
            
            // Check for auto-flush
            if (should_auto_flush()) {
                flush_current_thread();
            }
        }
        // Immediate mode: format and enqueue to async queue (non-blocking)
        else if (get_config().mode == TracingMode::Immediate) {
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            uint64_t now_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
            
            Event e;
            e.ts_ns = now_ns;
            e.func = current_func;
            e.file = file;
            e.line = line;
            e.tid = tid;
            e.color_offset = color_offset;
            e.type = EventType::Msg;
            e.depth = depth;
            e.dur_ns = 0;
            e.memory_rss = 0;
            
            if (!fmt) {
                e.msg[0] = 0;
            }
            else {
                int n = std::vsnprintf(e.msg, TRACE_MSG_CAP, fmt, ap);
                if (n < 0) {
                    e.msg[0] = 0;
                }
                else {
                    e.msg[std::min(n, TRACE_MSG_CAP)] = 0;
                }
            }
            
            // Enqueue to async queue (non-blocking, fast)
            async_queue().enqueue(e);
        }
        // Buffered mode: write to ring buffer only
        else {
            write(EventType::Msg, current_func, file, line);
            int buf_idx = 0;
#if TRACE_DOUBLE_BUFFER
            if (get_config().use_double_buffering) {
                buf_idx = active_buf.load(std::memory_order_relaxed);
            }
#endif
            Event& e = buf[buf_idx][(head[buf_idx] + TRACE_RING_CAP - 1) % TRACE_RING_CAP];
            
            if (!fmt) {
                e.msg[0] = 0;
                return;
            }
            
            int n = std::vsnprintf(e.msg, TRACE_MSG_CAP, fmt, ap);
            if (n < 0) {
                e.msg[0] = 0;
            }
            else {
                e.msg[std::min(n, TRACE_MSG_CAP)] = 0;
            }
        }
    }
};

/**
 * @brief Global registry of all thread-local ring buffers.
 * 
 * Tracks all active ring buffers for flush_all() operations.
 * Thread-safe access via mutex.
 * 
 * In DLL shared mode (when g_external_registry is set), the Registry also
 * manages heap-allocated Ring instances via thread_rings map to enable
 * proper sharing across DLL boundaries.
 */
struct Registry {
    std::mutex mtx;                 ///< Protects rings vector and thread_rings map
    std::vector<Ring*> rings;       ///< Pointers to all registered ring buffers
    std::map<std::thread::id, Ring*> thread_rings;  ///< Thread ID to Ring mapping for DLL sharing

    /**
     * @brief Register a new ring buffer.
     * @param r Pointer to ring buffer (must remain valid)
     */
    inline void add(Ring* r) {
        std::lock_guard<std::mutex> lock(mtx);
        rings.push_back(r);
    }
    
    /**
     * @brief Unregister a ring buffer (called from Ring destructor).
     * @param r Pointer to ring buffer to remove
     */
    inline void remove(Ring* r) {
        std::lock_guard<std::mutex> lock(mtx);
        rings.erase(std::remove(rings.begin(), rings.end(), r), rings.end());
    }
    
    /**
     * @brief Get or create Ring for current thread (DLL shared mode).
     * 
     * In DLL shared mode, Rings are heap-allocated and managed by the Registry
     * to ensure all DLLs access the same Ring per thread.
     * 
     * @return Pointer to Ring for current thread (never null)
     */
    inline Ring* get_or_create_thread_ring() {
        std::thread::id tid = std::this_thread::get_id();
        
        std::lock_guard<std::mutex> lock(mtx);
        
        // Check if Ring already exists for this thread
        auto it = thread_rings.find(tid);
        if (it != thread_rings.end()) {
            return it->second;
        }
        
        // Create new Ring on heap
        Ring* ring = new Ring();
        thread_rings[tid] = ring;
        rings.push_back(ring);  // Also add to flush list
        ring->registered = true;
        
        return ring;
    }
    
    /**
     * @brief Remove Ring for specific thread (DLL shared mode cleanup).
     * 
     * Called when a thread exits in DLL shared mode. Removes the Ring from
     * both the thread_rings map and the rings vector, then deletes it.
     * 
     * @param tid Thread ID to remove
     */
    inline void remove_thread_ring(std::thread::id tid) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = thread_rings.find(tid);
        if (it != thread_rings.end()) {
            Ring* ring = it->second;
            
            // Remove from both collections
            rings.erase(std::remove(rings.begin(), rings.end(), ring), rings.end());
            thread_rings.erase(it);
            
            // Delete the heap-allocated Ring
            delete ring;
        }
    }
};

/**
 * @brief Set external state for DLL-safe cross-boundary tracing (header-only solution).
 * 
 * Call this once from the main executable before any tracing occurs,
 * providing pointers to config and registry instances that live in
 * a shared location (e.g., main executable). All DLLs must call this
 * with the same instances to share trace state.
 * 
 * This is the recommended approach for DLL scenarios when you cannot
 * control which files are compiled (purely header-only solution).
 * 
 * When external state is set, the library automatically switches to
 * centralized Ring buffer management:
 * - Each thread's Ring is heap-allocated in the shared Registry
 * - All DLLs access the same Ring per thread (proper event sharing)
 * - Automatic cleanup when threads exit via ThreadRingGuard
 * 
 * @param cfg Pointer to shared Config instance (must outlive all tracing)
 * @param reg Pointer to shared Registry instance (must outlive all tracing)
 * 
 * Example:
 * @code
 * // In main.cpp or shared header:
 * static trace::Config g_trace_config;
 * static trace::Registry g_trace_registry;
 * 
 * int main() {
 *     trace::set_external_state(&g_trace_config, &g_trace_registry);
 *     // Now all DLLs share the same Config, Registry, and Ring buffers
 * }
 * @endcode
 */
inline void set_external_state(Config* cfg, Registry* reg) {
    g_external_config = cfg;
    g_external_registry = reg;
}

/**
 * @def TRACE_SETUP_DLL_SHARED()
 * @brief One-line DLL state sharing setup with automatic cleanup (RAII).
 * 
 * Creates shared Config and Registry instances, automatically registers them
 * before any tracing occurs, and flushes all traces on program exit via RAII.
 * 
 * This macro simplifies DLL state sharing from 20+ lines of boilerplate to
 * a single line. The RAII guard ensures automatic cleanup even if exceptions occur.
 * 
 * What gets shared across DLL boundaries:
 * - Config: All DLLs use the same configuration settings
 * - Registry: All DLLs register to the same central registry
 * - Ring buffers: Each thread's Ring is heap-allocated and shared across all DLLs
 * - Events: All trace events appear in unified output, regardless of which DLL generated them
 * 
 * Usage: Add this macro at the start of main() in your main executable.
 * 
 * Example:
 * @code
 * #include <trace-scope/trace_scope.hpp>
 * 
 * int main() {
 *     TRACE_SETUP_DLL_SHARED();  // One line - automatic setup & cleanup!
 *     
 *     // Configure tracing
 *     trace::config.out = std::fopen("trace.log", "w");
 *     
 *     // Use tracing normally across all DLLs
 *     TRACE_SCOPE();
 *     call_dll_functions();  // All DLLs share same trace state and Ring buffers
 *     
 *     return 0;  // Automatic flush on exit via RAII
 * }
 * @endcode
 * 
 * Benefits over manual setup:
 * - 1 line instead of 20+
 * - Automatic cleanup (no manual flush needed)
 * - Exception-safe (RAII guarantees cleanup)
 * - Less error-prone
 * - Proper Ring buffer sharing (fixed in v0.9.0-alpha)
 * 
 * Note: For advanced control, you can still use set_external_state() manually.
 */
#define TRACE_SETUP_DLL_SHARED_WITH_CONFIG(config_file) \
    static trace::Config g_trace_shared_config; \
    static trace::Registry g_trace_shared_registry; \
    static struct TraceDllGuard { \
        TraceDllGuard() { \
            trace::set_external_state(&g_trace_shared_config, &g_trace_shared_registry); \
            const char* cfg_path = (config_file); \
            if (cfg_path && cfg_path[0]) { \
                g_trace_shared_config.load_from_file(cfg_path); \
            } \
        } \
        ~TraceDllGuard() { \
            trace::flush_all(); \
        } \
    } g_trace_dll_guard

/**
 * @def TRACE_SETUP_DLL_SHARED()
 * @brief DLL state sharing setup without config file (backward compatible).
 * 
 * Same as TRACE_SETUP_DLL_SHARED_WITH_CONFIG(nullptr).
 */
#define TRACE_SETUP_DLL_SHARED() TRACE_SETUP_DLL_SHARED_WITH_CONFIG(nullptr)

/**
 * @brief Get the active config instance.
 * 
 * Returns external config if set via set_external_state(),
 * otherwise returns the default inline instance.
 * 
 * @return Reference to the active Config
 */
inline Config& get_config() {
    return g_external_config ? *g_external_config : config;
}

/**
 * @brief Config::load_from_file() implementation.
 * 
 * Parses INI file and applies configuration settings.
 */
inline bool Config::load_from_file(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) {
        std::fprintf(stderr, "trace-scope: Warning: Could not open config file: %s\n", path);
        return false;
    }
    
    std::string current_section;
    char line_buf[512];
    int line_num = 0;
    
    while (std::fgets(line_buf, sizeof(line_buf), f)) {
        ++line_num;
        std::string line = ini_parser::trim(line_buf);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Parse section header [section_name]
        if (line[0] == '[' && line[line.length()-1] == ']') {
            current_section = line.substr(1, line.length() - 2);
            current_section = ini_parser::trim(current_section);
            continue;
        }
        
        // Parse key = value
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            std::fprintf(stderr, "trace-scope: Warning: Invalid line in %s:%d (no '=')\n", path, line_num);
            continue;
        }
        
        std::string key = ini_parser::trim(line.substr(0, eq_pos));
        std::string value = ini_parser::trim(line.substr(eq_pos + 1));
        
        // Remove inline comments from value
        size_t comment_pos = value.find('#');
        if (comment_pos == std::string::npos) {
            comment_pos = value.find(';');
        }
        if (comment_pos != std::string::npos) {
            value = ini_parser::trim(value.substr(0, comment_pos));
        }
        
        // Apply configuration based on section and key
        if (current_section == "output") {
            if (key == "file") {
                FILE* new_out = std::fopen(ini_parser::unquote(value).c_str(), "w");
                if (new_out) {
                    if (out && out != stdout) std::fclose(out);
                    out = new_out;
                }
            }
            else if (key == "immediate_file") {
                FILE* new_imm = std::fopen(ini_parser::unquote(value).c_str(), "w");
                if (new_imm) {
                    if (immediate_out && immediate_out != stdout) std::fclose(immediate_out);
                    immediate_out = new_imm;
                }
            }
        }
        else if (current_section == "display") {
            if (key == "print_timing") print_timing = ini_parser::parse_bool(value);
            else if (key == "print_timestamp") print_timestamp = ini_parser::parse_bool(value);
            else if (key == "print_thread") print_thread = ini_parser::parse_bool(value);
            else if (key == "colorize_depth") colorize_depth = ini_parser::parse_bool(value);
            else if (key == "include_file_line") include_file_line = ini_parser::parse_bool(value);
            else if (key == "include_filename") include_filename = ini_parser::parse_bool(value);
            else if (key == "include_function_name") include_function_name = ini_parser::parse_bool(value);
            else if (key == "show_full_path") show_full_path = ini_parser::parse_bool(value);
        }
        else if (current_section == "performance") {
            if (key == "print_stats") print_stats = ini_parser::parse_bool(value);
            else if (key == "track_memory") track_memory = ini_parser::parse_bool(value);
        }
        else if (current_section == "dump") {
            if (key == "prefix") {
                static std::string prefix_str = ini_parser::unquote(value);
                dump_prefix = prefix_str.c_str();
            }
            else if (key == "suffix") {
                static std::string suffix_str = ini_parser::unquote(value);
                dump_suffix = suffix_str.c_str();
            }
            else if (key == "output_dir") {
                static std::string dir_str = ini_parser::unquote(value);
                output_dir = dir_str.c_str();
            }
            else if (key == "layout") {
                std::string layout_str = ini_parser::trim(value);
                // Convert to lowercase for case-insensitive comparison
                for (char& c : layout_str) {
                    c = (char)std::tolower((unsigned char)c);
                }
                if (layout_str == "flat") output_layout = OutputLayout::Flat;
                else if (layout_str == "date" || layout_str == "bydate") output_layout = OutputLayout::ByDate;
                else if (layout_str == "session" || layout_str == "bysession") output_layout = OutputLayout::BySession;
                else {
                    std::fprintf(stderr, "trace-scope: Warning: Unknown layout '%s' in %s:%d\n", 
                                value.c_str(), path, line_num);
                }
            }
            else if (key == "session") {
                current_session = ini_parser::parse_int(value);
            }
        }
        else if (current_section == "formatting") {
            if (key == "filename_width") filename_width = ini_parser::parse_int(value);
            else if (key == "line_width") line_width = ini_parser::parse_int(value);
            else if (key == "function_width") function_width = ini_parser::parse_int(value);
        }
        else if (current_section == "markers") {
            if (key == "show_indent_markers") show_indent_markers = ini_parser::parse_bool(value);
            else if (key == "indent_marker") {
                static std::string s_indent = ini_parser::unquote(value);
                indent_marker = s_indent.c_str();
            }
            else if (key == "enter_marker") {
                static std::string s_enter = ini_parser::unquote(value);
                enter_marker = s_enter.c_str();
            }
            else if (key == "exit_marker") {
                static std::string s_exit = ini_parser::unquote(value);
                exit_marker = s_exit.c_str();
            }
            else if (key == "message_marker") {
                static std::string s_msg = ini_parser::unquote(value);
                msg_marker = s_msg.c_str();
            }
        }
        else if (current_section == "modes") {
            if (key == "mode") {
                std::string m = ini_parser::trim(value);
                // Convert to lowercase for case-insensitive comparison
                for (char& c : m) {
                    c = (char)std::tolower((unsigned char)c);
                }
                if (m == "buffered") mode = TracingMode::Buffered;
                else if (m == "immediate") mode = TracingMode::Immediate;
                else if (m == "hybrid") mode = TracingMode::Hybrid;
                else {
                    std::fprintf(stderr, "trace-scope: Warning: Unknown mode '%s' in %s:%d\n", 
                                value.c_str(), path, line_num);
                }
            }
            else if (key == "auto_flush_at_exit") auto_flush_at_exit = ini_parser::parse_bool(value);
            else if (key == "use_double_buffering") use_double_buffering = ini_parser::parse_bool(value);
            else if (key == "auto_flush_threshold") auto_flush_threshold = ini_parser::parse_float(value);
            else if (key == "immediate_flush_interval_ms") immediate_flush_interval_ms = ini_parser::parse_int(value);
            else if (key == "immediate_queue_size") immediate_queue_size = (size_t)ini_parser::parse_int(value);
        }
        else if (current_section == "filter") {
            if (key == "include_function") {
                filter.include_functions.push_back(ini_parser::unquote(value));
            }
            else if (key == "exclude_function") {
                filter.exclude_functions.push_back(ini_parser::unquote(value));
            }
            else if (key == "include_file") {
                filter.include_files.push_back(ini_parser::unquote(value));
            }
            else if (key == "exclude_file") {
                filter.exclude_files.push_back(ini_parser::unquote(value));
            }
            else if (key == "max_depth") {
                filter.max_depth = ini_parser::parse_int(value);
            }
        }
    }
    
    std::fclose(f);
    return true;
}

/**
 * @brief Load configuration from file into global config.
 * 
 * Convenience function that calls get_config().load_from_file().
 * 
 * @param path Path to INI file
 * @return true on success, false if file not found
 * 
 * Example:
 * @code
 * trace::load_config("trace.conf");
 * TRACE_SCOPE();  // Now configured from file
 * @endcode
 */
inline bool load_config(const char* path) {
    return get_config().load_from_file(path);
}

/**
 * @brief Implementation of should_trace() - must be after get_config() is available.
 */
inline bool filter_utils::should_trace(const char* func, const char* file, int depth) {
    const auto& f = get_config().filter;
    
    // Check depth filter
    if (f.max_depth >= 0 && depth > f.max_depth) {
        return false;
    }
    
    // Check function filters
    if (func) {
        // If exclude list matches, filter out (exclude wins)
        if (matches_any(func, f.exclude_functions)) {
            return false;
        }
        
        // If include list is not empty and doesn't match, filter out
        if (!f.include_functions.empty() && !matches_any(func, f.include_functions)) {
            return false;
        }
    }
    
    // Check file filters
    if (file) {
        // If exclude list matches, filter out (exclude wins)
        if (matches_any(file, f.exclude_files)) {
            return false;
        }
        
        // If include list is not empty and doesn't match, filter out
        if (!f.include_files.empty() && !matches_any(file, f.include_files)) {
            return false;
        }
    }
    
    return true;  // Passed all filters
}

/**
 * @brief Add function include pattern (wildcard supported).
 * 
 * Only functions matching include patterns will be traced (unless excluded).
 * Empty include list means include all.
 * 
 * @param pattern Wildcard pattern (e.g., "my_namespace::*", "core_*")
 */
inline void filter_include_function(const char* pattern) {
    get_config().filter.include_functions.push_back(pattern);
}

/**
 * @brief Add function exclude pattern (wildcard supported).
 * 
 * Functions matching exclude patterns will never be traced.
 * Exclude takes priority over include.
 * 
 * @param pattern Wildcard pattern (e.g., "*_test", "debug_*")
 */
inline void filter_exclude_function(const char* pattern) {
    get_config().filter.exclude_functions.push_back(pattern);
}

/**
 * @brief Add file include pattern (wildcard supported).
 * 
 * Only files matching include patterns will be traced (unless excluded).
 * Empty include list means include all.
 * 
 * @param pattern Wildcard pattern (e.g., "src/core/ *", "*.cpp")
 */
inline void filter_include_file(const char* pattern) {
    get_config().filter.include_files.push_back(pattern);
}

/**
 * @brief Add file exclude pattern (wildcard supported).
 * 
 * Files matching exclude patterns will never be traced.
 * Exclude takes priority over include.
 * 
 * @param pattern Wildcard pattern (e.g., "* /test/ *", "*_generated.cpp")
 */
inline void filter_exclude_file(const char* pattern) {
    get_config().filter.exclude_files.push_back(pattern);
}

/**
 * @brief Set maximum trace depth.
 * 
 * Events beyond this depth will be filtered out.
 * Useful for limiting output from deep recursion.
 * 
 * @param depth Maximum depth (-1 = unlimited)
 */
inline void filter_set_max_depth(int depth) {
    get_config().filter.max_depth = depth;
}

/**
 * @brief Clear all filters (restore to trace everything).
 */
inline void filter_clear() {
    auto& f = get_config().filter;
    f.include_functions.clear();
    f.exclude_functions.clear();
    f.include_files.clear();
    f.exclude_files.clear();
    f.max_depth = -1;
}

#if defined(TRACE_SCOPE_SHARED)
// For DLL sharing: use extern/exported variable
TRACE_SCOPE_API Registry& registry();
#if defined(TRACE_SCOPE_IMPLEMENTATION)
/**
 * @brief Get the global registry instance (DLL-shared version).
 * 
 * Checks for external registry first (set via set_external_state()),
 * otherwise returns the static instance.
 * 
 * @return Reference to the active Registry
 */
Registry& registry() {
    if (g_external_registry) return *g_external_registry;
    static Registry r;
    return r;
}
#endif
#else
// For header-only: inline function with static local
/**
 * @brief Get the global registry instance (header-only version).
 * 
 * Checks for external registry first (set via set_external_state()),
 * otherwise returns the static instance.
 * 
 * @return Reference to the active Registry
 */
inline Registry& registry() {
    if (g_external_registry) return *g_external_registry;
    static Registry r;
    return r;
}
#endif

/**
 * @brief Get the global async queue for immediate mode.
 * 
 * Single global instance shared across all threads. The queue is started
 * automatically on first use in immediate or hybrid mode.
 * 
 * @return Reference to the global AsyncQueue
 */
inline AsyncQueue& async_queue() {
    static AsyncQueue q;
    return q;
}

/**
 * @brief Ring destructor implementation.
 * 
 * Defined here after registry() is available.
 * 
 * In DLL shared mode, Rings are heap-allocated and managed by Registry,
 * so we skip the unregistration here to avoid double-free. The Registry's
 * remove_thread_ring() handles cleanup in that case.
 */
inline Ring::~Ring() {
    // In centralized DLL mode, Registry manages cleanup via remove_thread_ring()
    // Don't unregister here as it would cause double-removal
    if (registered && !g_external_registry) {
        registry().remove(this);
        registered = false;
    }
}

/**
 * @brief Hash the current thread ID to a printable 32-bit value.
 * 
 * Uses a mixing function similar to MurmurHash3 to ensure good distribution
 * of thread IDs for display purposes.
 * 
 * @return 32-bit hash of the current thread ID
 */
inline uint32_t thread_id_hash() {
    auto id = std::this_thread::get_id();
    std::hash<std::thread::id> h;
    uint64_t v = (uint64_t)h(id);
    // mix into a printable 32-bit value
    v ^= (v >> 33); v *= 0xff51afd7ed558ccdULL;
    v ^= (v >> 33); v *= 0xc4ceb9fe1a85ec53ULL;
    v ^= (v >> 33);
    return (uint32_t)(v & 0xffffffffu);
}

/**
 * @brief Ring constructor: Initialize thread-specific values.
 */
inline Ring::Ring() {
    tid = thread_id_hash();
    color_offset = (uint8_t)(tid % 8);  // Assign color offset based on thread ID
}

/**
 * @brief RAII guard for cleaning up thread-local Ring in DLL shared mode.
 * 
 * When a thread exits in DLL shared mode, this destructor removes the Ring
 * from the centralized registry and deletes it. Only used in DLL mode.
 */
struct ThreadRingGuard {
    ~ThreadRingGuard() {
        if (g_external_registry) {
            // In DLL mode, remove this thread's Ring from registry
            g_external_registry->remove_thread_ring(std::this_thread::get_id());
        }
    }
};

/**
 * @brief Get the current thread's ring buffer.
 * 
 * In DLL shared mode (when g_external_registry is set), uses centralized
 * heap-allocated Ring storage to ensure all DLLs access the same Ring per thread.
 * 
 * In header-only mode, uses thread_local storage (each DLL gets its own copy).
 * 
 * @return Reference to the current thread's Ring
 */
inline Ring& thread_ring() {
    // DLL shared mode: use centralized Ring storage
    if (g_external_registry) {
        // Cache the pointer in thread_local storage to reduce overhead
        static thread_local Ring* cached_ring = nullptr;
        if (!cached_ring) {
            // Install cleanup guard (once per thread)
            static thread_local ThreadRingGuard cleanup_guard;
            cached_ring = g_external_registry->get_or_create_thread_ring();
        }
        return *cached_ring;
    }
    
    // Header-only mode: use thread_local storage (existing behavior)
    static thread_local Ring ring;
    static thread_local bool inited = false;
    if (!inited) {
        ring.tid = thread_id_hash();
        ring.color_offset = static_cast<uint8_t>(ring.tid % 8);
        registry().add(&ring);
        ring.registered = true;
        inited = true;
    }
    return ring;
}

/**
 * @brief Extract the basename from a file path.
 * 
 * Handles both Unix (/) and Windows (\) path separators.
 * 
 * @param p File path (can be null)
 * @return Pointer to the basename, or empty string if null
 */
inline const char* base_name(const char* p) {
    if (!p) return "";
    const char* s1 = std::strrchr(p, '/');
    const char* s2 = std::strrchr(p, '\\');
    const char* s  = (s1 && s2) ? (s1 > s2 ? s1 : s2) : (s1 ? s1 : s2);
    return s ? (s + 1) : p;
}

/**
 * @brief Print a single trace event to a file stream.
 * 
 * Formats the event according to the global config settings, including
 * optional timestamp, thread ID, file/line/function prefix, indentation,
 * and auto-scaled duration units.
 * 
 * @param e The event to print
 * @param out Output file stream
 */
inline void print_event(const Event& e, FILE* out) {
    // ANSI color for depth-based colorization with thread-aware offset
    if (get_config().colorize_depth) {
        // Combine depth and thread offset for visual distinction
        // Each thread gets a unique color offset, making multi-threaded traces easier to read
        int color_idx = (e.depth + e.color_offset) % 8;
        static const char* colors[] = {
            "\033[31m",  // Red
            "\033[32m",  // Green
            "\033[33m",  // Yellow
            "\033[34m",  // Blue
            "\033[35m",  // Magenta
            "\033[36m",  // Cyan
            "\033[37m",  // White
            "\033[91m"   // Bright Red
        };
        std::fprintf(out, "%s", colors[color_idx]);
    }
    
    if (get_config().print_timestamp) {
        // Convert ns timestamp to human-readable ISO format with milliseconds
        auto duration = std::chrono::nanoseconds(e.ts_ns);
        auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>(duration);
        auto time_t_val = std::chrono::system_clock::to_time_t(
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp));
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;
        
        std::tm tm_buf;
        #ifdef _WIN32
        localtime_s(&tm_buf, &time_t_val);
        #else
        localtime_r(&time_t_val, &tm_buf);
        #endif
        
        std::fprintf(out, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
            tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count());
    }
    if (get_config().print_thread)    std::fprintf(out, "(%08x) ", e.tid);

    // Filename:line:function prefix block (fixed widths), before indent so alignment is stable
    if (get_config().include_file_line && e.file) {
        bool printed_something = false;
        
        // Print filename if enabled
        if (get_config().include_filename) {
            const char* path = get_config().show_full_path ? e.file : base_name(e.file);
            const int fw = (get_config().filename_width > 0 ? get_config().filename_width : 20);
            
            // Head-truncate: show beginning of path (precision limits max chars printed)
            std::fprintf(out, "%-*.*s", fw, fw, path);
            printed_something = true;
        }
        
        // Print line number and function name if enabled (they're paired)
        if (get_config().include_function_name) {
            const int lw = (get_config().line_width > 0 ? get_config().line_width : 5);
            const int funcw = (get_config().function_width > 0 ? get_config().function_width : 20);
            const char* fname = e.func ? e.func : "";
            
            // Print colon separator if filename was printed
            if (printed_something) std::fprintf(out, ":");
            
            // Print line number
            std::fprintf(out, "%*d", lw, e.line);
            
            // Head-truncate function name: show beginning (precision limits max chars)
            std::fprintf(out, " %-*.*s", funcw, funcw, fname);
            printed_something = true;
        }
        
        if (printed_something) std::fprintf(out, " ");
    }

    // Depth indentation after prefix
    if (get_config().show_indent_markers) {
        // Show visual markers for each level
        const char* marker = get_config().indent_marker ? get_config().indent_marker : "| ";
        for (int i = 0; i < e.depth; ++i) {
            std::fputs(marker, out);
        }
    } else {
        // Plain whitespace indentation
        for (int i = 0; i < e.depth; ++i) {
            std::fputs("  ", out);
        }
    }

    // Event type markers
    const char* enter_mk = get_config().enter_marker ? get_config().enter_marker : "-> ";
    const char* exit_mk = get_config().exit_marker ? get_config().exit_marker : "<- ";
    const char* msg_mk = get_config().msg_marker ? get_config().msg_marker : "- ";

    switch (e.type) {
    case EventType::Enter:
        std::fprintf(out, "%s%s", enter_mk, e.func);
        break;
    case EventType::Exit:
        if (get_config().print_timing) {
            // Auto-scale units based on duration
            if (e.dur_ns < 1000ULL) {
                std::fprintf(out, "%s%s  [%llu ns]", exit_mk, e.func, (unsigned long long)e.dur_ns);
            } else if (e.dur_ns < 1000000ULL) {
                std::fprintf(out, "%s%s  [%.2f us]", exit_mk, e.func, e.dur_ns / 1000.0);
            } else if (e.dur_ns < 1000000000ULL) {
                std::fprintf(out, "%s%s  [%.2f ms]", exit_mk, e.func, e.dur_ns / 1000000.0);
            } else {
                std::fprintf(out, "%s%s  [%.3f s]", exit_mk, e.func, e.dur_ns / 1000000000.0);
            }
        } else {
            std::fprintf(out, "%s%s", exit_mk, e.func);
        }
        break;
    case EventType::Msg:
        std::fprintf(out, "%s%s", msg_mk, e.msg[0] ? e.msg : "");
        break;
    }
    
    // Reset color and add newline
    if (get_config().colorize_depth) {
        std::fprintf(out, "\033[0m");  // Reset to default color
    }
    std::fprintf(out, "\n");
}

/**
 * @brief Flush a single ring buffer to the output stream.
 * 
 * Prints all events in the ring buffer (handling wraparound). Thread-safe
 * via static mutex. If the ring has wrapped, only the most recent
 * TRACE_RING_CAP events are printed.
 * 
 * In double-buffering mode: atomically swaps buffers, flushes the old buffer,
 * then clears it for reuse. This allows writes to continue to the new active
 * buffer while flushing the old one without race conditions.
 * 
 * @param r The ring buffer to flush (non-const for double-buffering)
 */
inline void flush_ring(Ring& r) {
    static std::mutex io_mtx;
    FILE* out = get_config().out ? get_config().out : stdout;

    int flush_buf_idx = 0;
    uint32_t count = 0;
    uint32_t start = 0;
    
    // Double-buffering mode: swap buffers atomically
#if TRACE_DOUBLE_BUFFER
    if (get_config().use_double_buffering) {
        std::lock_guard<std::mutex> flush_lock(r.flush_mtx);
        
        // Atomically swap active buffer
        int old_buf = r.active_buf.load(std::memory_order_relaxed);
        int new_buf = 1 - old_buf;
        r.active_buf.store(new_buf, std::memory_order_release);
        
        // Now flush the old buffer (no one is writing to it)
        flush_buf_idx = old_buf;
        count = (r.wraps[flush_buf_idx] == 0) ? r.head[flush_buf_idx] : TRACE_RING_CAP;
        start = (r.wraps[flush_buf_idx] == 0) ? 0 : r.head[flush_buf_idx];
        
        // Print events from the flushed buffer
        {
            std::lock_guard<std::mutex> io_lock(io_mtx);
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t idx = (start + i) % TRACE_RING_CAP;
                const Event& e = r.buf[flush_buf_idx][idx];
                print_event(e, out);
            }
            std::fflush(out);
        }
        
        // Clear the flushed buffer for reuse
        r.head[flush_buf_idx] = 0;
        r.wraps[flush_buf_idx] = 0;
    }
    else
#endif
    // Single-buffer mode: flush in-place (original behavior)
    {
        std::lock_guard<std::mutex> io_lock(io_mtx);
        
        flush_buf_idx = 0;
        count = (r.wraps[flush_buf_idx] == 0) ? r.head[flush_buf_idx] : TRACE_RING_CAP;
        start = (r.wraps[flush_buf_idx] == 0) ? 0 : r.head[flush_buf_idx];
        
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t idx = (start + i) % TRACE_RING_CAP;
            const Event& e = r.buf[flush_buf_idx][idx];
            print_event(e, out);
        }
        std::fflush(out);
    }
}

/**
 * @brief Flush all registered ring buffers to the output stream.
 * 
 * Thread-safe. Takes a snapshot of all registered rings and flushes each one.
 * Call this manually or enable config.auto_flush_at_exit for automatic flushing.
 */
inline void flush_all() {
    std::vector<Ring*> snapshot;
    { 
        std::lock_guard<std::mutex> lock(registry().mtx); 
        snapshot = registry().rings; 
    }
    for (Ring* r : snapshot) {
        if (r && r->registered) {
            flush_ring(*r);
        }
    }
}

/**
 * @brief Flush only the current thread's ring buffer.
 * 
 * Used by hybrid mode for auto-flushing when buffer approaches capacity.
 * Only flushes the calling thread's buffer, not all threads.
 */
inline void flush_current_thread() {
    flush_ring(thread_ring());
}

/**
 * @brief Force immediate flush of async queue.
 * 
 * Blocks until all queued events in async immediate mode are written to file.
 * Only relevant when using Immediate or Hybrid mode (which use async queue).
 * 
 * Use this when synchronous semantics are needed:
 * - Before critical operations that might crash
 * - In test code to ensure output is written
 * - When switching output files
 * 
 * @note Waits up to 1 second for queue to drain, then times out with warning.
 * 
 * @example
 *   trace::config.mode = TracingMode::Immediate;
 *   TRACE_SCOPE();
 *   critical_operation();
 *   trace::flush_immediate_queue();  // Ensure events written before proceeding
 */
inline void flush_immediate_queue() {
    async_queue().flush_now();
}

/**
 * @brief Manually start async immediate mode with custom output.
 * 
 * Called automatically on first trace in Immediate or Hybrid mode, but can
 * be called explicitly to control startup timing or change output file.
 * 
 * @param out Output file (default: use config.out)
 * 
 * @example
 *   FILE* custom_out = fopen("immediate.log", "w");
 *   trace::start_async_immediate(custom_out);
 *   trace::config.mode = TracingMode::Immediate;
 *   TRACE_SCOPE();  // Uses custom_out
 */
inline void start_async_immediate(FILE* out = nullptr) {
    if (!out) out = get_config().out ? get_config().out : stdout;
    async_queue().flush_interval_ms = get_config().immediate_flush_interval_ms;
    async_queue().batch_size = get_config().immediate_queue_size;
    async_queue().start(out);
}

/**
 * @brief Stop async immediate mode and flush remaining events.
 * 
 * Stops the background writer thread and ensures all queued events are written.
 * Automatically called at program exit via atexit handler.
 * 
 * Useful for explicitly stopping async mode when switching modes or
 * shutting down tracing subsystem.
 */
inline void stop_async_immediate() {
    async_queue().stop();
}

/**
 * @brief Auto-flush when outermost scope exits.
 * 
 * Called by Scope destructor. If config.auto_flush_at_exit is true and
 * we're returning to depth 0, flushes all traces.
 * 
 * @param final_depth The depth after the scope exits
 */
inline void check_auto_flush_on_scope_exit(int final_depth) {
    if (get_config().auto_flush_at_exit && final_depth == 0) {
        flush_all();
    }
}

/**
 * @brief Dump all trace events to a compact binary file.
 * 
 * Binary format starts with "TRCLOG10" header followed by version info.
 * 
 * Version 2 format (current):
 *   Each event: type(1) + tid(4) + color_offset(1) + ts_ns(8) + depth(4) + 
 *               dur_ns(8) + memory_rss(8) + file_len(2) + file_str + func_len(2) + func_str +
 *               msg_len(2) + msg_str + line(4)
 * 
 * Use tools/trc_analyze.py to analyze the binary file.
 * The Python tool supports both version 1 and 2 formats.
 */

/**
 * @brief Generate timestamped filename for binary dumps.
 * 
 * Generates filename with configurable directory structure:
 * - Flat: output_dir/prefix_YYYYMMDD_HHMMSS_mmm.trc
 * - ByDate: output_dir/YYYY-MM-DD/prefix_HHMMSS_mmm.trc
 * - BySession: output_dir/session_NNN/prefix_YYYYMMDD_HHMMSS_mmm.trc
 * 
 * Creates directories automatically if they don't exist.
 * 
 * @param prefix Optional custom prefix (default: use Config::dump_prefix)
 * @return Full path to timestamped filename
 * 
 * @example
 *   trace::config.output_dir = "logs";
 *   trace::config.output_layout = trace::Config::OutputLayout::ByDate;
 *   auto filename = trace::generate_dump_filename();
 *   // Returns: "logs/2025-10-20/trace_162817_821.trc"
 */
inline std::string generate_dump_filename(const char* prefix = nullptr) {
    namespace fs = std::filesystem;
    
    if (!prefix) prefix = get_config().dump_prefix;
    const char* suffix = get_config().dump_suffix;
    
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t_val);
#else
    localtime_r(&time_t_val, &tm);
#endif
    
    // Build base path
    fs::path base_path;
    if (get_config().output_dir) {
        base_path = get_config().output_dir;
    } else {
        base_path = ".";
    }
    
    // Add subdirectory based on layout
    fs::path dir_path;
    switch (get_config().output_layout) {
        case Config::OutputLayout::ByDate: {
            // Subdirectory: YYYY-MM-DD
            char date_buf[32];
            std::snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d",
                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
            dir_path = base_path / date_buf;
            break;
        }
        case Config::OutputLayout::BySession: {
            // Subdirectory: session_NNN
            int session = get_config().current_session;
            
            // Auto-increment: find max existing session number
            if (session == 0) {
                int max_session = 0;
                try {
                    if (fs::exists(base_path)) {
                        for (const auto& entry : fs::directory_iterator(base_path)) {
                            if (entry.is_directory()) {
                                std::string dirname = entry.path().filename().string();
                                if (dirname.substr(0, 8) == "session_") {
                                    int num = std::atoi(dirname.substr(8).c_str());
                                    max_session = std::max(max_session, num);
                                }
                            }
                        }
                    }
                } catch (...) {
                    // Ignore errors during auto-detection
                }
                session = max_session + 1;
            }
            
            char session_buf[32];
            std::snprintf(session_buf, sizeof(session_buf), "session_%03d", session);
            dir_path = base_path / session_buf;
            break;
        }
        case Config::OutputLayout::Flat:
        default:
            // No subdirectory
            dir_path = base_path;
            break;
    }
    
    // Create directory if it doesn't exist
    try {
        if (!dir_path.empty() && !fs::exists(dir_path)) {
            fs::create_directories(dir_path);
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "trace-scope: Failed to create directory %s: %s\n",
                     dir_path.string().c_str(), e.what());
        // Fall back to current directory
        dir_path = ".";
    }
    
    // Generate filename
    char filename_buf[256];
    std::snprintf(filename_buf, sizeof(filename_buf), "%s_%04d%02d%02d_%02d%02d%02d_%03d%s",
                  prefix,
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec,
                  (int)ms.count(),
                  suffix);
    
    fs::path full_path = dir_path / filename_buf;
    return full_path.string();
}

/**
 * @brief Dump all ring buffers to a timestamped binary file.
 * 
 * Automatically generates timestamped filename with configurable directory structure.
 * Creates directories automatically if they don't exist.
 * 
 * @param prefix Optional custom prefix (default: Config::dump_prefix, which defaults to "trace")
 * @return Generated full path on success, empty string on failure
 * 
 * @example
 *   // Simple usage (current directory, flat layout)
 *   std::string filename = trace::dump_binary();
 *   // Returns: "trace_20251020_162817_821.trc"
 * 
 *   // With output directory and date-based layout
 *   trace::config.output_dir = "logs";
 *   trace::config.output_layout = trace::Config::OutputLayout::ByDate;
 *   std::string filename = trace::dump_binary();
 *   // Returns: "logs/2025-10-20/trace_162817_821.trc"
 * 
 *   // Session-based layout with auto-increment
 *   trace::config.output_layout = trace::Config::OutputLayout::BySession;
 *   std::string filename = trace::dump_binary("myapp");
 *   // Returns: "logs/session_001/myapp_20251020_162817_821.trc"
 */
inline std::string dump_binary(const char* prefix = nullptr) {
    std::string filename = generate_dump_filename(prefix);
    FILE* f = std::fopen(filename.c_str(), "wb");
    if (!f) return "";

    auto w8  = [&](uint8_t v){ std::fwrite(&v,1,1,f); };
    auto w16 = [&](uint16_t v){ std::fwrite(&v,1,2,f); };
    auto w32 = [&](uint32_t v){ std::fwrite(&v,1,4,f); };
    auto w64 = [&](uint64_t v){ std::fwrite(&v,1,8,f); };
    auto ws  = [&](const char* s, uint16_t n){ if (n) std::fwrite(s,1,n,f); };

    std::fwrite("TRCLOG10",1,8,f);
    w32(2); // version (bumped to 2 for color_offset field)
    w32(0);

    std::vector<Ring*> snapshot;
    { std::lock_guard<std::mutex> lock(registry().mtx); snapshot = registry().rings; }

    for (Ring* r : snapshot) {
        if (!r || !r->registered) continue;
        
        // Determine which buffer(s) to dump
        int num_buffers = TRACE_NUM_BUFFERS;
#if TRACE_DOUBLE_BUFFER
        if (!get_config().use_double_buffering) {
            num_buffers = 1;  // Only dump first buffer if not using double-buffering
        }
#endif
        
        for (int buf_idx = 0; buf_idx < num_buffers; ++buf_idx) {
            uint32_t count = (r->wraps[buf_idx] == 0) ? r->head[buf_idx] : TRACE_RING_CAP;
            uint32_t start = (r->wraps[buf_idx] == 0) ? 0 : r->head[buf_idx];

            for (uint32_t i = 0; i < count; ++i) {
                uint32_t idx = (start + i) % TRACE_RING_CAP;
                const Event& e = r->buf[buf_idx][idx];

                w8((uint8_t)e.type);
                w32(e.tid);
                w8(e.color_offset);  // Added in version 2 for thread-aware colors
                w64(e.ts_ns);
                w32((uint32_t)e.depth);
                w64(e.dur_ns);
                w64(e.memory_rss);  // Added in version 2 for memory tracking

                // file, func, msg as length-prefixed
                auto emit_str = [&](const char* s){
                    if (!s) { w16(0); return; }
                    uint16_t n = (uint16_t)std::min<size_t>(65535, std::strlen(s));
                    w16(n); ws(s, n);
                };
                emit_str(e.file);
                emit_str(e.func);
                emit_str(e.msg);
                w32((uint32_t)e.line);
            }
        }
    }
    std::fclose(f);
    return filename;
}

/**
 * @brief RAII scope guard for automatic function entry/exit tracing.
 * 
 * Constructor writes an Enter event, destructor writes an Exit event.
 * The destructor calculates duration and optionally triggers auto-flush
 * when returning to depth 0.
 * 
 * Use via the TRACE_SCOPE() macro, not directly.
 */
struct Scope {
    const char* func;  ///< Function name
    const char* file;  ///< Source file
    int         line;  ///< Source line
    
    /**
     * @brief Construct a scope guard and write Enter event.
     * @param f Function name
     * @param fi Source file
     * @param li Source line
     */
    inline Scope(const char* f, const char* fi, int li) : func(f), file(fi), line(li) {
#if TRACE_ENABLED
        thread_ring().write(EventType::Enter, func, file, line);
#endif
    }
    /**
     * @brief Destruct the scope guard and write Exit event.
     * 
     * Writes Exit event with calculated duration. If auto_flush_at_exit
     * is enabled and we're returning to depth 0, triggers flush_all().
     */
    inline ~Scope() {
#if TRACE_ENABLED
        Ring& r = thread_ring();
        r.write(EventType::Exit, func, file, line);
        // Auto-flush when returning to depth 0 (outermost scope exits)
        check_auto_flush_on_scope_exit(r.depth);
#endif
    }
};

/**
 * @brief Write a formatted trace message.
 * 
 * Creates a message event with printf-style formatting. The message is
 * associated with the current function (from the function stack).
 * 
 * Use via the TRACE_MSG() macro, not directly.
 * 
 * @param file Source file path
 * @param line Source line number
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
inline void trace_msgf(const char* file, int line, const char* fmt, ...) {
#if TRACE_ENABLED
    Ring& r = thread_ring();
    va_list ap;
    va_start(ap, fmt);
    r.write_msg(file, line, fmt, ap);
    va_end(ap);
#endif
}

/**
 * @brief Internal function for TRACE_ARG macro - with value.
 * 
 * Logs a function argument with its name, type, and value.
 * The value is formatted using operator<<.
 */
template<typename T>
inline void trace_arg(const char* file, int line, const char* name, const char* type_name, const T& value) {
#if TRACE_ENABLED
    std::ostringstream oss;
    oss << name << ": " << type_name << " = " << value;
    trace_msgf(file, line, "%s", oss.str().c_str());
#endif
}

/**
 * @brief Internal function for TRACE_ARG macro - without value (type only).
 * 
 * Logs a function argument with its name and type, but no value.
 * Used for non-printable types like custom classes.
 */
inline void trace_arg(const char* file, int line, const char* name, const char* type_name) {
#if TRACE_ENABLED
    std::ostringstream oss;
    oss << name << ": " << type_name;
    trace_msgf(file, line, "%s", oss.str().c_str());
#endif
}

/**
 * @brief Stream-based logging helper for C++ iostream-style output.
 * 
 * RAII helper that collects stream output via operator<< and writes
 * a trace message in the destructor. Provides a drop-in replacement
 * for stream-based logging macros.
 * 
 * Use via the TRACE_LOG macro, not directly.
 */
struct TraceStream {
    std::ostringstream ss;  ///< Stream buffer for collecting output
    const char* file;       ///< Source file
    int line;               ///< Source line
    
    /**
     * @brief Construct a stream logger.
     * @param f Source file path
     * @param l Source line number
     */
    TraceStream(const char* f, int l) : file(f), line(l) {}
    
    /**
     * @brief Destructor writes the collected stream to trace output.
     */
    ~TraceStream() {
#if TRACE_ENABLED
        trace_msgf(file, line, "%s", ss.str().c_str());
#endif
    }
    
    /**
     * @brief Stream insertion operator.
     * @tparam T Type of value to stream
     * @param val Value to append to the stream
     * @return Reference to this for chaining
     */
    template<typename T>
    TraceStream& operator<<(const T& val) {
        ss << val;
        return *this;
    }
};

/**
 * @brief Performance statistics computation and display.
 */
namespace stats {

/**
 * @brief Format duration in human-readable units.
 * 
 * @param ns Duration in nanoseconds
 * @return Formatted string (e.g., "1.23 ms", "456 µs", "2.5 s")
 */
inline std::string format_duration_str(uint64_t ns) {
    char buf[64];
    if (ns < 1000) {
        std::snprintf(buf, sizeof(buf), "%lu ns", (unsigned long)ns);
    } else if (ns < 1000000) {
        std::snprintf(buf, sizeof(buf), "%.2f µs", ns / 1000.0);
    } else if (ns < 1000000000) {
        std::snprintf(buf, sizeof(buf), "%.2f ms", ns / 1000000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%.3f s", ns / 1000000000.0);
    }
    return buf;
}

/**
 * @brief Format memory size in human-readable units.
 * 
 * @param bytes Memory size in bytes
 * @return Formatted string (e.g., "1.23 MB", "456 KB", "2.5 GB")
 */
inline std::string format_memory_str(uint64_t bytes) {
    char buf[64];
    if (bytes < 1024) {
        std::snprintf(buf, sizeof(buf), "%lu B", (unsigned long)bytes);
    } else if (bytes < 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.2f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

/**
 * @brief Compute performance statistics from all ring buffers.
 * 
 * Scans all registered ring buffers and computes per-function and per-thread
 * statistics including call counts, execution times, and memory usage.
 * 
 * @return Vector of per-thread statistics
 */
inline std::vector<ThreadStats> compute_stats() {
    std::vector<ThreadStats> result;
    std::lock_guard<std::mutex> lock(registry().mtx);
    
    // Map: tid → (func_name → stats)
    std::map<uint32_t, std::map<const char*, FunctionStats>> per_thread;
    std::map<uint32_t, uint64_t> thread_peak_rss;
    
    for (Ring* r : registry().rings) {
        if (!r || !r->registered) continue;
        
        uint32_t tid = r->tid;
        uint64_t thread_peak = 0;
        
        // Process all buffers for this ring
        int num_buffers = TRACE_NUM_BUFFERS;
#if TRACE_DOUBLE_BUFFER
        if (!get_config().use_double_buffering) {
            num_buffers = 1;  // Only process first buffer if not using double-buffering
        }
#endif
        
        for (int buf_idx = 0; buf_idx < num_buffers; ++buf_idx) {
            uint32_t count = (r->wraps[buf_idx] == 0) ? r->head[buf_idx] : TRACE_RING_CAP;
            uint32_t start = (r->wraps[buf_idx] == 0) ? 0 : r->head[buf_idx];
            
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t idx = (start + i) % TRACE_RING_CAP;
                const Event& e = r->buf[buf_idx][idx];
                
                // Track peak RSS for this thread
                if (e.memory_rss > 0) {
                    thread_peak = std::max(thread_peak, e.memory_rss);
                }
                
                // Only track Exit events (they have duration)
                if (e.type != EventType::Exit || !e.func) continue;
                
                auto& stats = per_thread[tid][e.func];
                if (stats.call_count == 0) {
                    stats.func_name = e.func;
                    stats.min_ns = UINT64_MAX;
                    stats.max_ns = 0;
                    stats.memory_delta = 0;
                }
                
                stats.call_count++;
                stats.total_ns += e.dur_ns;
                stats.min_ns = std::min(stats.min_ns, e.dur_ns);
                stats.max_ns = std::max(stats.max_ns, e.dur_ns);
                
                // Track memory delta (simplified: use RSS at exit)
                if (e.memory_rss > 0) {
                    stats.memory_delta = std::max(stats.memory_delta, e.memory_rss);
                }
            }
        }
        
        thread_peak_rss[tid] = thread_peak;
    }
    
    // Convert to vector
    for (auto& [tid, funcs] : per_thread) {
        ThreadStats ts;
        ts.tid = tid;
        ts.total_events = 0;
        ts.peak_rss = thread_peak_rss[tid];
        
        for (auto& [fname, fstats] : funcs) {
            ts.functions.push_back(fstats);
            ts.total_events += fstats.call_count;
        }
        result.push_back(ts);
    }
    
    return result;
}

/**
 * @brief Print performance statistics to output stream.
 * 
 * Displays global and per-thread statistics in a formatted table.
 * Shows function call counts, execution times, and memory usage.
 * 
 * @param out Output stream (default: stderr)
 */
inline void print_stats(FILE* out = stderr) {
    auto stats = compute_stats();
    if (stats.empty()) return;
    
    std::fprintf(out, "\n");
    std::fprintf(out, "================================================================================\n");
    std::fprintf(out, " Performance Metrics Summary\n");
    std::fprintf(out, "================================================================================\n");
    
    // Global aggregation
    std::map<const char*, FunctionStats> global;
    uint64_t global_peak_rss = 0;
    
    for (const auto& ts : stats) {
        global_peak_rss = std::max(global_peak_rss, ts.peak_rss);
        
        for (const auto& fs : ts.functions) {
            auto& g = global[fs.func_name];
            if (g.call_count == 0) {
                g.func_name = fs.func_name;
                g.min_ns = UINT64_MAX;
                g.max_ns = 0;
                g.memory_delta = 0;
            }
            g.call_count += fs.call_count;
            g.total_ns += fs.total_ns;
            g.min_ns = std::min(g.min_ns, fs.min_ns);
            g.max_ns = std::max(g.max_ns, fs.max_ns);
            g.memory_delta = std::max(g.memory_delta, fs.memory_delta);
        }
    }
    
    // Sort by total time descending
    std::vector<FunctionStats> sorted;
    for (auto& [name, fs] : global) sorted.push_back(fs);
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.total_ns > b.total_ns;
    });
    
    // Print global stats
    std::fprintf(out, "\nGlobal Statistics:\n");
    std::fprintf(out, "--------------------------------------------------------------------------------\n");
    std::fprintf(out, "%-40s %10s %12s %12s %12s %12s %12s\n",
                 "Function", "Calls", "Total", "Avg", "Min", "Max", "Memory");
    std::fprintf(out, "--------------------------------------------------------------------------------\n");
    
    for (const auto& fs : sorted) {
        std::fprintf(out, "%-40s %10lu %12s %12s %12s %12s %12s\n",
                     fs.func_name,
                     (unsigned long)fs.call_count,
                     format_duration_str(fs.total_ns).c_str(),
                     format_duration_str((uint64_t)fs.avg_ns()).c_str(),
                     format_duration_str(fs.min_ns).c_str(),
                     format_duration_str(fs.max_ns).c_str(),
                     format_memory_str(fs.memory_delta).c_str());
    }
    
    // Print system memory summary
    if (global_peak_rss > 0) {
        std::fprintf(out, "\nSystem Memory Summary:\n");
        std::fprintf(out, "--------------------------------------------------------------------------------\n");
        std::fprintf(out, "Peak RSS: %s\n", format_memory_str(global_peak_rss).c_str());
        std::fprintf(out, "Current RSS: %s\n", format_memory_str(memory_utils::get_current_rss()).c_str());
    }
    
    // Per-thread breakdown (if multiple threads)
    if (stats.size() > 1) {
        std::fprintf(out, "\nPer-Thread Breakdown:\n");
        std::fprintf(out, "================================================================================\n");
        
        for (const auto& ts : stats) {
            std::fprintf(out, "\nThread 0x%08x (%lu events, peak RSS: %s):\n", 
                         ts.tid, (unsigned long)ts.total_events, 
                         format_memory_str(ts.peak_rss).c_str());
            std::fprintf(out, "--------------------------------------------------------------------------------\n");
            std::fprintf(out, "%-40s %10s %12s %12s %12s\n", "Function", "Calls", "Total", "Avg", "Memory");
            std::fprintf(out, "--------------------------------------------------------------------------------\n");
            
            // Sort by total time
            auto thread_sorted = ts.functions;
            std::sort(thread_sorted.begin(), thread_sorted.end(), [](const auto& a, const auto& b) {
                return a.total_ns > b.total_ns;
            });
            
            for (const auto& fs : thread_sorted) {
                std::fprintf(out, "%-40s %10lu %12s %12s %12s\n",
                             fs.func_name,
                             (unsigned long)fs.call_count,
                             format_duration_str(fs.total_ns).c_str(),
                             format_duration_str((uint64_t)fs.avg_ns()).c_str(),
                             format_memory_str(fs.memory_delta).c_str());
            }
        }
    }
    
    std::fprintf(out, "================================================================================\n\n");
}

} // namespace stats

/**
 * @brief Automatic exit statistics using atexit().
 * 
 * Registers a function to print performance statistics at program exit
 * when Config::print_stats is enabled.
 */
namespace internal {

// Flag to ensure we only register once
static bool stats_registered = false;

// Exit handler function
inline void stats_exit_handler() {
    if (get_config().print_stats) {
        stats::print_stats(get_config().out ? get_config().out : stderr);
    }
}

// Register exit handler if stats are enabled
inline void ensure_stats_registered() {
    if (!stats_registered && get_config().print_stats) {
        std::atexit(stats_exit_handler);
        stats_registered = true;
    }
}

} // namespace internal

} // namespace trace

/**
 * @def TRACE_SCOPE()
 * @brief Trace the current function scope.
 * 
 * Creates an RAII scope guard that logs function entry immediately and
 * function exit (with duration) when the scope ends. Automatically handles
 * nesting depth for proper indentation.
 * 
 * Example:
 * @code
 * void my_function() {
 *     TRACE_SCOPE();  // Logs entry and exit automatically
 *     // ... function body ...
 * }
 * @endcode
 */
#define TRACE_SCOPE() ::trace::Scope _trace_scope_obj(__func__, __FILE__, __LINE__)

/**
 * @def TRACE_MSG(fmt, ...)
 * @brief Log a formatted message within the current scope.
 * 
 * Uses printf-style format strings. The message is associated with the
 * current function and displayed at the current indentation depth.
 * 
 * Example:
 * @code
 * TRACE_MSG("Processing item %d of %d", current, total);
 * @endcode
 * 
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
#define TRACE_MSG(...) ::trace::trace_msgf(__FILE__, __LINE__, __VA_ARGS__)

/**
 * @def TRACE_LOG
 * @brief Stream-based logging within the current scope.
 * 
 * Provides C++ iostream-style logging using operator<<. Drop-in replacement
 * for stream-based logging macros. The message is associated with the
 * current function and displayed at the current indentation depth.
 * 
 * Example:
 * @code
 * int id = 42;
 * std::string name = "test";
 * TRACE_LOG << "Processing item " << id << ", name=" << name;
 * 
 * // Drop-in replacement for:
 * // KY_COUT("Processing item " << id << ", name=" << name);
 * @endcode
 */
#define TRACE_LOG ::trace::TraceStream(__FILE__, __LINE__)

/**
 * @brief Helper function to format containers for TRACE_ARG.
 * 
 * Formats container contents as a single-line string with up to max_elem elements,
 * followed by "..." if more elements exist. Output format: [elem1, elem2, elem3, ...]
 * 
 * @tparam Container Any container type with begin()/end() iterators
 * @param c The container to format
 * @param max_elem Maximum number of elements to show (default: 5)
 * @return Formatted string representation of the container
 */
template<typename Container>
inline std::string format_container(const Container& c, size_t max_elem = 5) {
    std::ostringstream oss;
    oss << "[";
    size_t count = 0;
    for (const auto& item : c) {
        if (count > 0) oss << ", ";
        if (count >= max_elem) {
            oss << "...";
            break;
        }
        oss << item;
        count++;
    }
    oss << "]";
    return oss.str();
}

/**
 * @def TRACE_CONTAINER(container, max_elements)
 * @brief Helper macro to format containers for TRACE_ARG.
 * 
 * Shows up to max_elements from the container, then "..." if more exist.
 * Single-line output: [elem1, elem2, elem3, ...]
 * 
 * Example:
 * @code
 * std::vector<int> values = {1, 2, 3, 4, 5, 6, 7};
 * TRACE_ARG("values", std::vector<int>, TRACE_CONTAINER(values, 5));
 * // Output: values: std::vector<int> = [1, 2, 3, 4, 5, ...]
 * @endcode
 * 
 * @param container The container to format
 * @param max_elements Maximum number of elements to display
 */
#define TRACE_CONTAINER(container, max_elements) ::trace::format_container(container, max_elements)

/**
 * @def TRACE_ARG(name, type, ...)
 * @brief Log a function argument with its name, type, and optionally its value.
 * 
 * Used to automatically log function parameters. Can be used with or without
 * the value parameter. For printable types, include the value. For complex
 * types, omit the value.
 * 
 * Example:
 * @code
 * void process(int id, const std::vector<int>& values, MyClass& obj) {
 *     TRACE_SCOPE();
 *     TRACE_ARG("id", int, id);  // Printable type with value
 *     TRACE_ARG("values", std::vector<int>, TRACE_CONTAINER(values, 5));  // Container
 *     TRACE_ARG("obj", MyClass);  // Non-printable type, no value
 * }
 * @endcode
 * 
 * @param name String literal with the parameter name
 * @param type Type of the parameter
 * @param ... Optional value or formatted value (for printable types)
 */
#define TRACE_ARG(...) ::trace::trace_arg(__FILE__, __LINE__, __VA_ARGS__)
