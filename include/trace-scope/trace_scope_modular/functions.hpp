#ifndef FUNCTIONS_HPP
#define FUNCTIONS_HPP

/**
 * @file functions.hpp
 * @brief Function implementations
 */

#include <cstdio>
#include <cstdarg>
#include <sstream>
#include <atomic>
#include <mutex>
#include <vector>
#include <thread>

namespace trace {

// Forward declarations
namespace dll_shared_state {
    class Registry;
    Registry* get_shared_registry();
}

/**
 * @brief RAII guard for thread-local ring cleanup in DLL shared mode.
 * 
 * Automatically removes the thread's ring from the shared registry
 * when the thread exits.
 */
struct ThreadRingGuard {
    ~ThreadRingGuard() {
        if (dll_shared_state::get_shared_registry()) {
            dll_shared_state::get_shared_registry()->remove_thread_ring(std::this_thread::get_id());
        }
    }
};

// Forward declarations
inline Config& get_config();
inline Ring& thread_ring();
inline Registry& registry();

inline FILE* safe_fopen(const char* filename, const char* mode) {
#ifdef _MSC_VER
    FILE* file = nullptr;
    if (fopen_s(&file, filename, mode) != 0) {
        return nullptr;
    }
    return file;
#else
    return std::fopen(filename, mode);
#endif
}

inline FILE* safe_tmpfile() {
#ifdef _MSC_VER
    FILE* file = nullptr;
    if (tmpfile_s(&file) != 0) {
        return nullptr;
    }
    return file;
#else
    return std::tmpfile();
#endif
}

// async_queue() implementation moved below

// get_config() implementation moved below

inline void flush_current_thread() {
    flush_ring(thread_ring());
}

inline bool should_use_shared_memory() {
    Config& cfg = get_config();
    
    if (cfg.shared_memory_mode == SharedMemoryMode::DISABLED) {
        return false;
    }
    if (cfg.shared_memory_mode == SharedMemoryMode::ENABLED) {
        return true;
    }
    
    // AUTO mode: detect if shared memory already exists
    std::string shm_name = shared_memory::get_shared_memory_name();
    auto handle = shared_memory::create_or_open_shared_memory(
        shm_name.c_str(),
        sizeof(dll_shared_state::SharedTraceState),
        false  // try to open existing
    );
    
    bool exists = handle.valid;
    if (exists) {
        shared_memory::close_shared_memory(handle);
    }
    return exists;
}

inline void set_external_state(Config* cfg, Registry* reg) {
    dll_shared_state::set_shared_config(cfg);
    dll_shared_state::set_shared_registry(reg);
}

inline Config& get_config() {
    return dll_shared_state::get_shared_config() ? *dll_shared_state::get_shared_config() : config;
}

inline bool load_config(const char* path) {
    return get_config().load_from_file(path);
}

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

inline void filter_include_function(const char* pattern) {
    get_config().filter.include_functions.push_back(pattern);
}

inline void filter_exclude_function(const char* pattern) {
    get_config().filter.exclude_functions.push_back(pattern);
}

inline void filter_include_file(const char* pattern) {
    get_config().filter.include_files.push_back(pattern);
}

inline void filter_exclude_file(const char* pattern) {
    get_config().filter.exclude_files.push_back(pattern);
}

inline void filter_set_max_depth(int depth) {
    get_config().filter.max_depth = depth;
}

inline void filter_clear() {
    auto& f = get_config().filter;
    f.include_functions.clear();
    f.exclude_functions.clear();
    f.include_files.clear();
    f.exclude_files.clear();
    f.max_depth = -1;
}

// registry() implementation moved below

inline AsyncQueue& async_queue() {
    static AsyncQueue q;
    return q;
}

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
    // Check if we should use shared mode
    if (dll_shared_state::get_shared_registry() || should_use_shared_memory()) {
        // Shared memory path (existing logic)
        static thread_local Ring* cached_ring = nullptr;
        if (!cached_ring) {
            static thread_local ThreadRingGuard cleanup_guard;
            cached_ring = dll_shared_state::get_shared_registry()->get_or_create_thread_ring();
        }
        return *cached_ring;
    }
    
    // Thread-local path (existing logic)
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

inline const char* base_name(const char* p) {
    if (!p) return "";
    const char* s1 = std::strrchr(p, '/');
    const char* s2 = std::strrchr(p, '\\');
    const char* s  = (s1 && s2) ? (s1 > s2 ? s1 : s2) : (s1 ? s1 : s2);
    return s ? (s + 1) : p;
}

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

inline void flush_ring(Ring& r) {
    static std::mutex io_mtx;
    FILE* out = get_config().out ? get_config().out : stdout;

    int flush_buf_idx = 0;
    uint32_t count = 0;
    uint32_t start = 0;
    
    // Double-buffering mode: swap buffers atomically
#if TRC_DOUBLE_BUFFER
    if (get_config().use_double_buffering) {
        std::lock_guard<std::mutex> flush_lock(r.flush_mtx);
        
        // Atomically swap active buffer
        int old_buf = r.active_buf.load(std::memory_order_relaxed);
        int new_buf = 1 - old_buf;
        r.active_buf.store(new_buf, std::memory_order_release);
        
        // Now flush the old buffer (no one is writing to it)
        flush_buf_idx = old_buf;
        count = (r.wraps[flush_buf_idx] == 0) ? r.head[flush_buf_idx] : TRC_RING_CAP;
        start = (r.wraps[flush_buf_idx] == 0) ? 0 : r.head[flush_buf_idx];
        
        // Print events from the flushed buffer
        {
            std::lock_guard<std::mutex> io_lock(io_mtx);
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t idx = (start + i) % TRC_RING_CAP;
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
        count = (r.wraps[flush_buf_idx] == 0) ? r.head[flush_buf_idx] : TRC_RING_CAP;
        start = (r.wraps[flush_buf_idx] == 0) ? 0 : r.head[flush_buf_idx];
        
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t idx = (start + i) % TRC_RING_CAP;
            const Event& e = r.buf[flush_buf_idx][idx];
            print_event(e, out);
        }
        std::fflush(out);
    }
}

inline void flush_current_thread() {
    flush_ring(thread_ring());
}

inline void flush_immediate_queue() {
    async_queue().flush_now();
}

inline void start_async_immediate(FILE* out = nullptr) {
    if (!out) out = get_config().out ? get_config().out : stdout;
    async_queue().flush_interval_ms = get_config().immediate_flush_interval_ms;
    async_queue().batch_size = get_config().immediate_queue_size;
    async_queue().start(out);
}

inline void stop_async_immediate() {
    async_queue().stop();
}

inline void check_auto_flush_on_scope_exit(int final_depth) {
    if (get_config().auto_flush_at_exit && final_depth == 0) {
        flush_all();
    }
}

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

inline std::string dump_binary(const char* prefix = nullptr) {
    std::string filename = generate_dump_filename(prefix);
    FILE* f = safe_fopen(filename.c_str(), "wb");
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
        int num_buffers = TRC_NUM_BUFFERS;
#if TRC_DOUBLE_BUFFER
        if (!get_config().use_double_buffering) {
            num_buffers = 1;  // Only dump first buffer if not using double-buffering
        }
#endif
        
        for (int buf_idx = 0; buf_idx < num_buffers; ++buf_idx) {
            uint32_t count = (r->wraps[buf_idx] == 0) ? r->head[buf_idx] : TRC_RING_CAP;
            uint32_t start = (r->wraps[buf_idx] == 0) ? 0 : r->head[buf_idx];

            for (uint32_t i = 0; i < count; ++i) {
                uint32_t idx = (start + i) % TRC_RING_CAP;
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

inline void trace_msgf(const char* file, int line, const char* fmt, ...) {
#if TRC_ENABLED
    Ring& r = thread_ring();
    va_list ap;
    va_start(ap, fmt);
    r.write_msg(file, line, fmt, ap);
    va_end(ap);
#endif
}

inline void trace_arg(const char* file, int line, const char* name, const char* type_name) {
#if TRC_ENABLED
    std::ostringstream oss;
    oss << name << ": " << type_name;
    trace_msgf(file, line, "%s", oss.str().c_str());
#endif
}

inline void set_external_state(Config* cfg, Registry* reg) {
    if (cfg) {
        dll_shared_state::set_shared_config(cfg);
    }
    if (reg) {
        dll_shared_state::set_shared_registry(reg);
    }
}

inline bool load_config(const char* path) {
    return get_config().load_from_file(path);
}

inline void dump_stats() {
    // Implementation for dumping statistics
    // This would be implemented based on the original header
}

inline void ensure_stats_registered() {
    if (!stats_registered.load()) {
        stats_registered.store(true);
        // Register with stats system
    }
}

// Forward declarations for missing functions
struct TraceStream;
uint32_t thread_id_hash();


// Missing function implementations
inline Ring& thread_ring() {
    // Check if we should use shared mode
    if (dll_shared_state::get_shared_registry() || should_use_shared_memory()) {
        // Shared memory path (existing logic)
        static thread_local Ring* cached_ring = nullptr;
        if (!cached_ring) {
            static thread_local ThreadRingGuard cleanup_guard;
            cached_ring = dll_shared_state::get_shared_registry()->get_or_create_thread_ring();
        }
        return *cached_ring;
    }
    
    // Thread-local path (existing logic)
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

inline void flush_ring(Ring& r) {
    static std::mutex io_mtx;
    FILE* out = get_config().out ? get_config().out : stdout;

    int flush_buf_idx = 0;
    uint32_t count = 0;
    uint32_t start = 0;
    
    // Double-buffering mode: swap buffers atomically
#if TRC_DOUBLE_BUFFER
    if (get_config().use_double_buffering) {
        std::lock_guard<std::mutex> flush_lock(r.flush_mtx);
        
        // Atomically swap active buffer
        int old_buf = r.active_buf.load(std::memory_order_relaxed);
        int new_buf = 1 - old_buf;
        r.active_buf.store(new_buf, std::memory_order_release);
        
        // Now flush the old buffer (no one is writing to it)
        flush_buf_idx = old_buf;
        count = (r.wraps[flush_buf_idx] == 0) ? r.head[flush_buf_idx] : TRC_RING_CAP;
        start = (r.wraps[flush_buf_idx] == 0) ? 0 : r.head[flush_buf_idx];
        
        // Print events from the flushed buffer
        {
            std::lock_guard<std::mutex> io_lock(io_mtx);
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t idx = (start + i) % TRC_RING_CAP;
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
        count = (r.wraps[flush_buf_idx] == 0) ? r.head[flush_buf_idx] : TRC_RING_CAP;
        start = (r.wraps[flush_buf_idx] == 0) ? 0 : r.head[flush_buf_idx];
        
        for (uint32_t i = 0; i < count; ++i) {
            uint32_t idx = (start + i) % TRC_RING_CAP;
            const Event& e = r.buf[flush_buf_idx][idx];
            print_event(e, out);
        }
        std::fflush(out);
    }
}

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

inline void trace_msgf(const char* file, int line, const char* fmt, ...) {
#if TRC_ENABLED
    Ring& r = thread_ring();
    va_list ap;
    va_start(ap, fmt);
    r.write_msg(file, line, fmt, ap);
    va_end(ap);
#endif
}

// TraceStream implementation
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
#if TRC_ENABLED
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

// Registry accessor
inline Registry& registry() {
    static Registry reg;
    return reg;
}

// Thread ID hash function
inline uint32_t thread_id_hash() {
    static std::atomic<uint32_t> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace trace

#endif // FUNCTIONS_HPP