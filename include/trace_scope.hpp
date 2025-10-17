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

struct Config {
    FILE* out = stdout;
    bool print_timing = true;
    bool print_timestamp = false;     // show timestamp (opt-in)
    bool print_thread = true;
    bool auto_flush_at_exit = false;  // automatically flush at program exit (opt-in)
    bool immediate_mode = false;      // bypass ring buffer, print immediately (opt-in, for long-running processes)

    // When true we include filename:line in the left prefix
    bool include_file_line = true;

    // NEW: filename rendering options
    bool include_filename = true;     // show filename in prefix
    bool show_full_path = false;      // default OFF -> show only basename
    int  filename_width = 20;         // fixed width for filename column
    int  line_width     = 5;          // fixed width for line number
    
    // NEW: function name rendering options
    bool include_function_name = true;  // show function name in prefix (line number pairs with this)
    int  function_width = 20;           // fixed width for function name column
};
TRACE_SCOPE_VAR Config config;

enum class EventType : uint8_t { Enter = 0, Exit = 1, Msg = 2 };

struct Event {
    uint64_t   ts_ns;                  ///< System clock ns (wall-clock time)
    const char* func;                  ///< Function name (enter/exit; null for msg)
    const char* file;                  ///< Source file (for all events)
    int         line;                  ///< Source line (for all events)
    int         depth;                 ///< Indentation depth at event
    uint32_t    tid;                   ///< Thread id (hashed printable)
    EventType   type;                  ///< Enter / Exit / Msg
    uint64_t    dur_ns;                ///< Exit duration; 0 otherwise
    char        msg[TRACE_MSG_CAP + 1];///< Msg payload if type==Msg, else ""
};

// Forward declaration for immediate mode
inline void print_event(const Event& e, FILE* out);

struct Ring {
    Event     buf[TRACE_RING_CAP];
    uint32_t  head = 0;
    uint64_t  wraps = 0;
    int       depth = 0;
    uint32_t  tid   = 0;
    bool      registered = false;
    uint64_t  start_stack[TRACE_DEPTH_MAX]; // start time per depth for durations
    const char* func_stack[TRACE_DEPTH_MAX]; // function name per depth for messages

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
        if (config.immediate_mode) {
            static std::mutex io_mtx;
            std::lock_guard<std::mutex> lock(io_mtx);
            FILE* out = config.out ? config.out : stdout;
            print_event(e, out);
            std::fflush(out);
        } else {
            // Buffered mode: write to ring buffer
            buf[head] = e;
            head = (head + 1) % TRACE_RING_CAP;
            if (head == 0) ++wraps;
        }
    }

    inline void write_msg(const char* file, int line, const char* fmt, va_list ap) {
        // Get current function name from the stack (depth is at current scope)
        const char* current_func = nullptr;
        int d = depth > 0 ? depth - 1 : 0;
        if (d < TRACE_DEPTH_MAX) {
            current_func = func_stack[d];
        }
        
        if (config.immediate_mode) {
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
            FILE* out = config.out ? config.out : stdout;
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

struct Registry {
    std::mutex mtx;
    std::vector<Ring*> rings;

    inline void add(Ring* r) {
        std::lock_guard<std::mutex> lock(mtx);
        rings.push_back(r);
    }
};

#if defined(TRACE_SCOPE_SHARED)
// For DLL sharing: use extern/exported variable
TRACE_SCOPE_API Registry& registry();
#if defined(TRACE_SCOPE_IMPLEMENTATION)
Registry& registry() {
    static Registry r;
    return r;
}
#endif
#else
// For header-only: inline function with static local
inline Registry& registry() {
    static Registry r;
    return r;
}
#endif

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

// Helper: get basename from a path (handles '/' and '\')
inline const char* base_name(const char* p) {
    if (!p) return "";
    const char* s1 = std::strrchr(p, '/');
    const char* s2 = std::strrchr(p, '\\');
    const char* s  = (s1 && s2) ? (s1 > s2 ? s1 : s2) : (s1 ? s1 : s2);
    return s ? (s + 1) : p;
}

inline void print_event(const Event& e, FILE* out) {
    if (config.print_timestamp) {
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
    if (config.print_thread)    std::fprintf(out, "(%08x) ", e.tid);

    // Filename:line:function prefix block (fixed widths), before indent so alignment is stable
    if (config.include_file_line && e.file) {
        bool printed_something = false;
        
        // Print filename if enabled
        if (config.include_filename) {
            const char* path = config.show_full_path ? e.file : base_name(e.file);
            const int fw = (config.filename_width > 0 ? config.filename_width : 20);
            
            // Head-truncate: show beginning of path (precision limits max chars printed)
            std::fprintf(out, "%-*.*s", fw, fw, path);
            printed_something = true;
        }
        
        // Print line number and function name if enabled (they're paired)
        if (config.include_function_name) {
            const int lw = (config.line_width > 0 ? config.line_width : 5);
            const int funcw = (config.function_width > 0 ? config.function_width : 20);
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
    for (int i = 0; i < e.depth; ++i) std::fputs("  ", out);

    switch (e.type) {
    case EventType::Enter:
        std::fprintf(out, "-> %s\n", e.func);
        break;
    case EventType::Exit:
        if (config.print_timing) {
            // Auto-scale units based on duration
            if (e.dur_ns < 1000ULL) {
                std::fprintf(out, "<- %s  [%llu ns]\n", e.func, (unsigned long long)e.dur_ns);
            } else if (e.dur_ns < 1000000ULL) {
                std::fprintf(out, "<- %s  [%.2f µs]\n", e.func, e.dur_ns / 1000.0);
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

inline void flush_ring(const Ring& r) {
    static std::mutex io_mtx;
    std::lock_guard<std::mutex> guard(io_mtx);
    FILE* out = config.out ? config.out : stdout;

    uint32_t count = (r.wraps == 0) ? r.head : TRACE_RING_CAP;
    uint32_t start = (r.wraps == 0) ? 0 : r.head;

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t idx = (start + i) % TRACE_RING_CAP;
        const Event& e = r.buf[idx];
        print_event(e, out);
    }
    std::fflush(out);
}

inline void flush_all() {
    std::vector<Ring*> snapshot;
    { std::lock_guard<std::mutex> lock(registry().mtx); snapshot = registry().rings; }
    for (Ring* r : snapshot) if (r && r->registered) flush_ring(*r);
}

// Auto-flush when outermost scope exits (typically main)
inline void check_auto_flush_on_scope_exit(int final_depth) {
    if (config.auto_flush_at_exit && final_depth == 0) {
        flush_all();
    }
}

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

struct Scope {
    const char* func;
    const char* file;
    int         line;
    inline Scope(const char* f, const char* fi, int li) : func(f), file(fi), line(li) {
#if TRACE_ENABLED
        thread_ring().write(EventType::Enter, func, file, line);
#endif
    }
    inline ~Scope() {
#if TRACE_ENABLED
        Ring& r = thread_ring();
        r.write(EventType::Exit, func, file, line);
        // Auto-flush when returning to depth 0 (outermost scope exits)
        check_auto_flush_on_scope_exit(r.depth);
#endif
    }
};

inline void trace_msgf(const char* file, int line, const char* fmt, ...) {
#if TRACE_ENABLED
    Ring& r = thread_ring();
    va_list ap;
    va_start(ap, fmt);
    r.write_msg(file, line, fmt, ap);
    va_end(ap);
#endif
}

} // namespace trace

#define TRACE_SCOPE() ::trace::Scope _trace_scope_obj(__func__, __FILE__, __LINE__)
#define TRACE_MSG(...) ::trace::trace_msgf(__FILE__, __LINE__, __VA_ARGS__)
