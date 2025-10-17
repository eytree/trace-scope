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

#include <atomic>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
#include <sstream>

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

namespace trace {

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
    bool immediate_mode = false;      ///< Bypass ring buffer, print immediately (opt-in, for long-running processes)

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
    
    // Indentation visualization
    bool show_indent_markers = true;    ///< Show visual markers for indentation levels (| for depth)
};
TRACE_SCOPE_VAR Config config;

// Forward declarations for external state system
struct Registry;
inline Config& get_config();

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
    EventType   type;                   ///< Event type (Enter/Exit/Msg)
    uint64_t    dur_ns;                 ///< Duration in nanoseconds (Exit only; 0 otherwise)
    char        msg[TRACE_MSG_CAP + 1]; ///< Message text (Msg events only; empty otherwise)
};

// Forward declaration for immediate mode
inline void print_event(const Event& e, FILE* out);

/**
 * @brief Per-thread ring buffer for trace events.
 * 
 * Each thread gets its own Ring (thread_local storage). Events are written
 * lock-free to the ring buffer. When the buffer fills, oldest events are
 * overwritten (wraps counter increments).
 */
struct Ring {
    Event       buf[TRACE_RING_CAP];                ///< Circular buffer of events
    uint32_t    head = 0;                           ///< Next write position
    uint64_t    wraps = 0;                          ///< Number of buffer wraparounds
    int         depth = 0;                          ///< Current call stack depth
    uint32_t    tid   = 0;                          ///< Thread ID (cached)
    bool        registered = false;                 ///< Whether this ring is registered globally
    uint64_t    start_stack[TRACE_DEPTH_MAX];       ///< Start timestamp per depth (for duration calculation)
    const char* func_stack[TRACE_DEPTH_MAX];        ///< Function name per depth (for message context)

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
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t now_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

        Event e;
        e.ts_ns = now_ns;
        e.func  = func;
        e.file  = file;
        e.line  = line;
        e.tid   = tid;
        e.type  = type;
        e.msg[0]= '\0';
        e.dur_ns= 0;

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

        // Immediate mode: print directly instead of buffering
        if (get_config().immediate_mode) {
            static std::mutex io_mtx;
            std::lock_guard<std::mutex> lock(io_mtx);
            FILE* out = get_config().out ? get_config().out : stdout;
            print_event(e, out);
            std::fflush(out);
        } else {
            // Buffered mode: write to ring buffer
            buf[head] = e;
            head = (head + 1) % TRACE_RING_CAP;
            if (head == 0) ++wraps;
        }
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
        
        if (get_config().immediate_mode) {
            // Immediate mode: format and print directly
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            uint64_t now_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
            
            Event e;
            e.ts_ns = now_ns;
            e.func = current_func;
            e.file = file;
            e.line = line;
            e.tid = tid;
            e.type = EventType::Msg;
            e.depth = depth;
            e.dur_ns = 0;
            
            if (!fmt) { e.msg[0] = 0; } 
            else {
                int n = std::vsnprintf(e.msg, TRACE_MSG_CAP, fmt, ap);
                if (n < 0) e.msg[0] = 0;
                else e.msg[std::min(n, TRACE_MSG_CAP)] = 0;
            }
            
            static std::mutex io_mtx;
            std::lock_guard<std::mutex> lock(io_mtx);
            FILE* out = get_config().out ? get_config().out : stdout;
            print_event(e, out);
            std::fflush(out);
        } else {
            // Buffered mode: write to ring buffer
            write(EventType::Msg, current_func, file, line);
            Event& e = buf[(head + TRACE_RING_CAP - 1) % TRACE_RING_CAP];
            if (!fmt) { e.msg[0] = 0; return; }
            int n = std::vsnprintf(e.msg, TRACE_MSG_CAP, fmt, ap);
            if (n < 0) e.msg[0] = 0;
            else e.msg[std::min(n, TRACE_MSG_CAP)] = 0;
        }
    }
};

/**
 * @brief Global registry of all thread-local ring buffers.
 * 
 * Tracks all active ring buffers for flush_all() operations.
 * Thread-safe access via mutex.
 */
struct Registry {
    std::mutex mtx;                 ///< Protects rings vector
    std::vector<Ring*> rings;       ///< Pointers to all registered ring buffers

    /**
     * @brief Register a new ring buffer.
     * @param r Pointer to ring buffer (must remain valid)
     */
    inline void add(Ring* r) {
        std::lock_guard<std::mutex> lock(mtx);
        rings.push_back(r);
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
 *     // Now all DLLs share the same state
 * }
 * @endcode
 */
inline void set_external_state(Config* cfg, Registry* reg) {
    g_external_config = cfg;
    g_external_registry = reg;
}

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
 * @brief Get the current thread's ring buffer.
 * 
 * Each thread gets its own ring buffer (thread_local storage). On first call,
 * initializes the ring and registers it with the global registry.
 * 
 * @return Reference to the current thread's Ring
 */
inline Ring& thread_ring() {
    static thread_local Ring ring;
    static thread_local bool inited = false;
    if (!inited) {
        ring.tid = thread_id_hash();
        ring.registered = true;
        registry().add(&ring);
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
        for (int i = 0; i < e.depth; ++i) {
            std::fputs("| ", out);
        }
    } else {
        // Plain whitespace indentation
        for (int i = 0; i < e.depth; ++i) {
            std::fputs("  ", out);
        }
    }

    switch (e.type) {
    case EventType::Enter:
        std::fprintf(out, "-> %s\n", e.func);
        break;
    case EventType::Exit:
        if (get_config().print_timing) {
            // Auto-scale units based on duration
            if (e.dur_ns < 1000ULL) {
                std::fprintf(out, "<- %s  [%llu ns]\n", e.func, (unsigned long long)e.dur_ns);
            } else if (e.dur_ns < 1000000ULL) {
                std::fprintf(out, "<- %s  [%.2f us]\n", e.func, e.dur_ns / 1000.0);
            } else if (e.dur_ns < 1000000000ULL) {
                std::fprintf(out, "<- %s  [%.2f ms]\n", e.func, e.dur_ns / 1000000.0);
            } else {
                std::fprintf(out, "<- %s  [%.3f s]\n", e.func, e.dur_ns / 1000000000.0);
            }
        } else {
            std::fprintf(out, "<- %s\n", e.func);
        }
        break;
    case EventType::Msg:
        std::fprintf(out, "- %s\n", e.msg[0] ? e.msg : "");
        break;
    }
}

/**
 * @brief Flush a single ring buffer to the output stream.
 * 
 * Prints all events in the ring buffer (handling wraparound). Thread-safe
 * via static mutex. If the ring has wrapped, only the most recent
 * TRACE_RING_CAP events are printed.
 * 
 * @param r The ring buffer to flush
 */
inline void flush_ring(const Ring& r) {
    static std::mutex io_mtx;
    std::lock_guard<std::mutex> guard(io_mtx);
    FILE* out = get_config().out ? get_config().out : stdout;

    uint32_t count = (r.wraps == 0) ? r.head : TRACE_RING_CAP;
    uint32_t start = (r.wraps == 0) ? 0 : r.head;

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t idx = (start + i) % TRACE_RING_CAP;
        const Event& e = r.buf[idx];
        print_event(e, out);
    }
    std::fflush(out);
}

/**
 * @brief Flush all registered ring buffers to the output stream.
 * 
 * Thread-safe. Takes a snapshot of all registered rings and flushes each one.
 * Call this manually or enable config.auto_flush_at_exit for automatic flushing.
 */
inline void flush_all() {
    std::vector<Ring*> snapshot;
    { std::lock_guard<std::mutex> lock(registry().mtx); snapshot = registry().rings; }
    for (Ring* r : snapshot) if (r && r->registered) flush_ring(*r);
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
 * Each event is serialized with its type, thread ID, timestamp, depth,
 * duration, and length-prefixed strings (file, func, msg).
 * 
 * Use tools/trc_pretty.py to pretty-print the binary file.
 * 
 * @param path Output file path
 * @return true on success, false on failure
 */
inline bool dump_binary(const char* path) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;

    auto w8  = [&](uint8_t v){ std::fwrite(&v,1,1,f); };
    auto w16 = [&](uint16_t v){ std::fwrite(&v,1,2,f); };
    auto w32 = [&](uint32_t v){ std::fwrite(&v,1,4,f); };
    auto w64 = [&](uint64_t v){ std::fwrite(&v,1,8,f); };
    auto ws  = [&](const char* s, uint16_t n){ if (n) std::fwrite(s,1,n,f); };

    std::fwrite("TRCLOG10",1,8,f);
    w32(1); // version
    w32(0);

    std::vector<Ring*> snapshot;
    { std::lock_guard<std::mutex> lock(registry().mtx); snapshot = registry().rings; }

    for (Ring* r : snapshot) {
        if (!r || !r->registered) continue;
        uint32_t count = (r->wraps == 0) ? r->head : TRACE_RING_CAP;
        uint32_t start = (r->wraps == 0) ? 0 : r->head;

        for (uint32_t i = 0; i < count; ++i) {
            uint32_t idx = (start + i) % TRACE_RING_CAP;
            const Event& e = r->buf[idx];

            w8((uint8_t)e.type);
            w32(e.tid);
            w64(e.ts_ns);
            w32((uint32_t)e.depth);
            w64(e.dur_ns);

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
    std::fclose(f);
    return true;
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
