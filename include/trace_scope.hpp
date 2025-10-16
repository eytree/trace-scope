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
 *
 * Build-time defines:
 *   TRACE_ENABLED   (default 1)
 *   TRACE_RING_CAP  (default 4096)  // events per thread
 *   TRACE_MSG_CAP   (default 192)   // max message size
 *   TRACE_DEPTH_MAX (default 512)   // max nesting depth tracked for durations
 */

#include <atomic>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>

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
    bool print_timestamp = true;
    bool print_thread = true;
    bool include_file_line = true;
};
inline Config config;

enum class EventType : uint8_t { Enter = 0, Exit = 1, Msg = 2 };

struct Event {
    uint64_t   ts_ns;                  ///< Steady clock ns
    const char* func;                  ///< Function name (enter/exit; null for msg)
    const char* file;                  ///< Source file (for all events)
    int         line;                  ///< Source line (for all events)
    int         depth;                 ///< Indentation depth at event
    uint32_t    tid;                   ///< Thread id (hashed printable)
    EventType   type;                  ///< Enter / Exit / Msg
    uint64_t    dur_ns;                ///< Exit duration; 0 otherwise
    char        msg[TRACE_MSG_CAP + 1];///< Msg payload if type==Msg, else ""
};

struct Ring {
    Event     buf[TRACE_RING_CAP];
    uint32_t  head = 0;
    uint64_t  wraps = 0;
    int       depth = 0;
    uint32_t  tid   = 0;
    bool      registered = false;
    uint64_t  start_stack[TRACE_DEPTH_MAX]; // start time per depth for durations

    inline void write(EventType type, const char* func, const char* file, int line) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        uint64_t now_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

        Event& e = buf[head];
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
            if (d < TRACE_DEPTH_MAX) start_stack[d] = now_ns;
            ++depth;
        } else { // Exit
            if (depth > 0) --depth;
            int d = depth;
            e.depth = d;
            if (d < TRACE_DEPTH_MAX) {
                uint64_t start_ns = start_stack[d];
                e.dur_ns = now_ns - start_ns;
            }
        }

        head++;
        if (head >= TRACE_RING_CAP) { head = 0; ++wraps; }
    }

    inline void write_msg(const char* file, int line, const char* fmt, va_list ap) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        uint64_t now_ns = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

        Event& e = buf[head];
        e.ts_ns = now_ns;
        e.func  = nullptr;
        e.file  = file;
        e.line  = line;
        e.depth = depth;
        e.tid   = tid;
        e.type  = EventType::Msg;
        e.dur_ns= 0;

        if (fmt) {
            std::vsnprintf(e.msg, sizeof(e.msg), fmt, ap);
            e.msg[TRACE_MSG_CAP] = '\0';
        } else {
            e.msg[0] = '\0';
        }

        head++;
        if (head >= TRACE_RING_CAP) { head = 0; ++wraps; }
    }
};

struct Registry {
    std::mutex mtx;
    std::vector<Ring*> rings;
    void add(Ring* r) {
        std::lock_guard<std::mutex> lock(mtx);
        rings.push_back(r);
    }
};

inline Registry& registry() {
    static Registry R;
    return R;
}

inline uint32_t thread_id_hash() {
    auto id = std::this_thread::get_id();
    std::hash<std::thread::id> h;
    return static_cast<uint32_t>(h(id));
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

inline void print_event(const Event& e, FILE* out) {
    if (config.print_timestamp) std::fprintf(out, "[%llu] ", (unsigned long long)e.ts_ns);
    if (config.print_thread)    std::fprintf(out, "(tid=%08x) ", e.tid);

    for (int i = 0; i < e.depth; ++i) std::fputs("  ", out);

    switch (e.type) {
    case EventType::Enter:
        if (config.include_file_line && e.file)
            std::fprintf(out, "-> %s (%s:%d)\n", e.func, e.file, e.line);
        else
            std::fprintf(out, "-> %s\n", e.func);
        break;
    case EventType::Exit:
        if (config.include_file_line && e.file) {
            if (config.print_timing)
                std::fprintf(out, "<-  %s  [%llu µs] (%s:%d)\n",
                    e.func, (unsigned long long)(e.dur_ns/1000ULL), e.file, e.line);
            else
                std::fprintf(out, "<-  %s (%s:%d)\n", e.func, e.file, e.line);
        } else {
            if (config.print_timing)
                std::fprintf(out, "<-  %s  [%llu µs]\n",
                    e.func, (unsigned long long)(e.dur_ns/1000ULL));
            else
                std::fprintf(out, "<-  %s\n", e.func);
        }
        break;
    case EventType::Msg:
        if (config.include_file_line && e.file)
            std::fprintf(out, ". %s  (%s:%d)\n", e.msg[0] ? e.msg : "", e.file, e.line);
        else
            std::fprintf(out, ". %s\n", e.msg[0] ? e.msg : "");
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
        const Event& e = r.buf[(start + i) % TRACE_RING_CAP];
        print_event(e, out);
    }
    std::fflush(out);
}

inline void flush_current_thread() {
    flush_ring(thread_ring());
}

inline void flush_all() {
    std::vector<Ring*> snapshot;
    { std::lock_guard<std::mutex> lock(registry().mtx); snapshot = registry().rings; }
    for (Ring* r : snapshot) if (r && r->registered) flush_ring(*r);
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

        w32(count);
        for (uint32_t i=0;i<count;++i) {
            const Event& e = r->buf[(start + i) % TRACE_RING_CAP];
            const char* func = (e.func ? e.func : "");
            const char* file = (e.file ? e.file : "");
            const char* msg  = e.msg;

            uint16_t func_len = (uint16_t)std::min<size_t>(65535, std::strlen(func));
            uint16_t file_len = (uint16_t)std::min<size_t>(65535, std::strlen(file));
            uint16_t msg_len  = (uint16_t)std::min<size_t>(65535, std::strlen(msg));

            w64(e.ts_ns);
            w32(e.tid);
            w8 (static_cast<uint8_t>(e.type));
            w32(e.depth);
            w64(e.dur_ns);
            w32((uint32_t)e.line);

            w16(func_len); w16(file_len); w16(msg_len);
            ws(func, func_len); ws(file, file_len); ws(msg, msg_len);
        }
    }
    std::fclose(f);
    return true;
}

struct Scope {
    const char* func;
    const char* file;
    int line;
    Scope(const char* f, const char* fl, int ln) : func(f), file(fl), line(ln) {
#if TRACE_ENABLED
        thread_ring().write(EventType::Enter, func, file, line);
#endif
    }
    ~Scope() {
#if TRACE_ENABLED
        thread_ring().write(EventType::Exit,  func, file, line);
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
