#ifndef TRACE_SCOPE_FUNCTIONS_HPP
#define TRACE_SCOPE_FUNCTIONS_HPP

/**
 * @file functions.hpp
 * @brief All function implementations for trace-scope
 */

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <functional>
#include <condition_variable>

namespace trace {

// Forward declarations
struct Config;
struct Registry;
struct AsyncQueue;
struct Ring;
struct Event;
struct FunctionStats;
struct ThreadStats;

// Forward declarations for utility functions
namespace filter_utils {
    inline bool should_trace(const char* func, const char* file, int depth) {
        // Simple implementation - always trace for now
        return true;
    }
}

namespace memory_utils {
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
}

namespace dll_shared_state {
    inline Registry* get_shared_registry() {
        // Simple implementation - return nullptr for now (no shared state)
        return nullptr;
    }
}

// Global variable declarations (from variables.hpp)
extern Config config;

// Essential utility functions
inline uint32_t thread_id_hash() {
    auto id = std::this_thread::get_id();
    return static_cast<uint32_t>(std::hash<std::thread::id>{}(id));
}

inline const char* base_name(const char* p) {
    if (!p) return "";
    
    const char* last_slash = std::strrchr(p, '/');
    const char* last_backslash = std::strrchr(p, '\\');
    
    const char* last_sep = last_slash > last_backslash ? last_slash : last_backslash;
    return last_sep ? last_sep + 1 : p;
}

// Configuration functions
inline Config& get_config() {
    return config;  // Simplified for now
}

// Registry functions - defined in variables.hpp

// Async queue functions - defined in variables.hpp

// Thread ring functions
inline Ring& thread_ring() {
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

// Flush functions
inline void flush_ring(Ring& r) {
    static std::mutex io_mtx;
    std::lock_guard<std::mutex> lock(io_mtx);
    
    Config& cfg = get_config();
    FILE* out = cfg.out ? cfg.out : stdout;
    
    // Simple flush implementation
    uint32_t start = 0;
    uint32_t count = r.head[0];
    
    if (r.wraps[0] > 0) {
        start = r.head[0];
        count = TRC_RING_CAP;
    }
    
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t idx = (start + i) % TRC_RING_CAP;
        print_event(r.buf[0][idx], out);
    }
    
    // Reset buffer
    r.head[0] = 0;
    r.wraps[0] = 0;
    
    std::fflush(out);
}

inline void flush_all() {
    std::vector<Ring*> snapshot;
    {
        std::lock_guard<std::mutex> lock(registry().mtx);
        snapshot = registry().rings;
    }
    
    for (Ring* r : snapshot) {
        if (r) {
            flush_ring(*r);
        }
    }
}

inline void flush_current_thread() {
    flush_ring(thread_ring());
}

// Binary dump functions
inline std::string generate_dump_filename(const char* prefix = nullptr) {
    Config& cfg = get_config();
    if (!prefix) prefix = cfg.dump_prefix;
    
    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&time_t));
    
    // Generate filename
    std::string filename = prefix;
    filename += "_";
    filename += timestamp;
    filename += "_";
    filename += std::to_string(ms.count());
    filename += cfg.dump_suffix;
    
    return filename;
}

inline std::string dump_binary(const char* prefix = nullptr) {
    std::string filename = generate_dump_filename(prefix);
    
    Config& cfg = get_config();
    FILE* f = safe_fopen(filename.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "trace-scope: Error: Could not create dump file: %s\n", filename.c_str());
        return "";
    }
    
    // Write header
    std::fprintf(f, "TRC_BINARY_V1\n");
    std::fprintf(f, "VERSION=%s\n", TRC_SCOPE_VERSION);
    std::fprintf(f, "TIMESTAMP=%llu\n", 
        static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()));
    
    std::fclose(f);
    return filename;
}

// Ring method implementations
inline Ring::Ring() {
    tid = thread_id_hash();
    color_offset = (uint8_t)(tid % 8);  // Assign color offset based on thread ID
}

inline Ring::~Ring() {
    // In centralized DLL mode, Registry manages cleanup via remove_thread_ring()
    // Don't unregister here as it would cause double-removal
    if (registered && !dll_shared_state::get_shared_registry()) {
        registry().remove(this);
        registered = false;
    }
}

inline bool Ring::should_auto_flush() const {
    if (get_config().mode != TracingMode::Hybrid) {
        return false;
    }
    
    // Check active buffer usage
    int buf_idx = 0;
#if TRC_DOUBLE_BUFFER
    if (get_config().use_double_buffering) {
        buf_idx = active_buf.load(std::memory_order_relaxed);
    }
#endif
    float usage = (float)head[buf_idx] / (float)TRC_RING_CAP;
    if (wraps[buf_idx] > 0) {
        usage = 1.0f;  // Already wrapped = 100% full
    }
    
    return usage >= get_config().auto_flush_threshold;
}

inline void Ring::write(EventType type, const char* func, const char* file, int line) {
#if TRC_ENABLED
        // Apply filters - skip if filtered out, but still update depth
        if (!filter_utils::should_trace(func, file, depth)) {
            // Must still track depth to maintain correct nesting
            if (type == EventType::Enter) {
                int d = depth;
                if (d < TRC_DEPTH_MAX) {
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
            if (d < TRC_DEPTH_MAX) {
                start_stack[d] = now_ns;
                func_stack[d] = func;  // Track function name for messages
            }
            ++depth;
        } else if (type == EventType::Exit) {
            depth = std::max(0, depth - 1);
            int d = std::max(0, depth);
            e.depth = d;
            if (d < TRC_DEPTH_MAX) {
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
#if TRC_DOUBLE_BUFFER
            if (get_config().use_double_buffering) {
                buf_idx = active_buf.load(std::memory_order_relaxed);
            }
#else
            if (get_config().use_double_buffering) {
                static bool warned = false;
                if (!warned) {
                    std::fprintf(stderr, "trace-scope: ERROR: use_double_buffering=true but not compiled with TRC_DOUBLE_BUFFER=1\n");
                    std::fprintf(stderr, "trace-scope: Recompile with -DTRC_DOUBLE_BUFFER=1 or add before include\n");
                    warned = true;
                }
            }
#endif
            buf[buf_idx][head[buf_idx]] = e;
            head[buf_idx] = (head[buf_idx] + 1) % TRC_RING_CAP;
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
#if TRC_DOUBLE_BUFFER
            if (get_config().use_double_buffering) {
                buf_idx = active_buf.load(std::memory_order_relaxed);
            }
#endif
            buf[buf_idx][head[buf_idx]] = e;
            head[buf_idx] = (head[buf_idx] + 1) % TRC_RING_CAP;
            if (head[buf_idx] == 0) {
                ++wraps[buf_idx];
            }
        }
#endif
    }

inline void Ring::write_msg(const char* file, int line, const char* fmt, va_list ap) {
        // Get current function name from the stack (depth is at current scope)
        const char* current_func = nullptr;
        int d = depth > 0 ? depth - 1 : 0;
        if (d < TRC_DEPTH_MAX) {
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
                int n = std::vsnprintf(e.msg, TRC_MSG_CAP, fmt, ap);
                if (n < 0) {
                    e.msg[0] = 0;
                }
                else {
                    e.msg[std::min(n, TRC_MSG_CAP)] = 0;
                }
            }
            
            // Write to buffer
            int buf_idx = 0;
#if TRC_DOUBLE_BUFFER
            if (get_config().use_double_buffering) {
                buf_idx = active_buf.load(std::memory_order_relaxed);
            }
#endif
            buf[buf_idx][head[buf_idx]] = e;
            head[buf_idx] = (head[buf_idx] + 1) % TRC_RING_CAP;
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
                int n = std::vsnprintf(e.msg, TRC_MSG_CAP, fmt, ap);
                if (n < 0) {
                    e.msg[0] = 0;
                }
                else {
                    e.msg[std::min(n, TRC_MSG_CAP)] = 0;
                }
            }
            
            // Enqueue to async queue (non-blocking, fast)
            async_queue().enqueue(e);
        }
        // Buffered mode: write to ring buffer only
        else {
            write(EventType::Msg, current_func, file, line);
            int buf_idx = 0;
#if TRC_DOUBLE_BUFFER
            if (get_config().use_double_buffering) {
                buf_idx = active_buf.load(std::memory_order_relaxed);
            }
#endif
            Event& e = buf[buf_idx][(head[buf_idx] + TRC_RING_CAP - 1) % TRC_RING_CAP];
            
            if (!fmt) {
                e.msg[0] = 0;
                return;
            }
            
            int n = std::vsnprintf(e.msg, TRC_MSG_CAP, fmt, ap);
            if (n < 0) {
                e.msg[0] = 0;
            }
            else {
                e.msg[std::min(n, TRC_MSG_CAP)] = 0;
            }
        }
    }

// Print event function
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

// Message functions
inline void trace_msgf(const char* file, int line, const char* fmt, ...) {
#if TRC_ENABLED
    va_list ap;
    va_start(ap, fmt);
    thread_ring().write_msg(file, line, fmt, ap);
    va_end(ap);
#endif
}

template<typename T>
inline void trace_arg(const char* file, int line, const char* name, const char* type_name, const T& value) {
#if TRC_ENABLED
    std::ostringstream oss;
    oss << name << "[" << type_name << "]=" << value;
    trace_msgf(file, line, "%s", oss.str().c_str());
#endif
}

inline void trace_arg(const char* file, int line, const char* name, const char* type_name) {
#if TRC_ENABLED
    trace_msgf(file, line, "%s[%s]=<unknown>", name, type_name);
#endif
}

// Missing functions that were in original header
inline void set_external_state(Config* cfg, Registry* reg) {
    dll_shared_state::set_shared_config(cfg);
    dll_shared_state::set_shared_registry(reg);
}

inline bool load_config(const char* path) {
    return get_config().load_from_file(path);
}

inline void filter_include_function(const char* pattern) {
    config.filter.include_functions.push_back(pattern);
}

inline void filter_exclude_function(const char* pattern) {
    config.filter.exclude_functions.push_back(pattern);
}

inline void filter_include_file(const char* pattern) {
    config.filter.include_files.push_back(pattern);
}

inline void filter_exclude_file(const char* pattern) {
    config.filter.exclude_files.push_back(pattern);
}

inline void filter_set_max_depth(int depth) {
    config.filter.max_depth = depth;
}

inline void filter_clear() {
    config.filter.include_functions.clear();
    config.filter.exclude_functions.clear();
    config.filter.include_files.clear();
    config.filter.exclude_files.clear();
    config.filter.max_depth = -1;
}

inline void flush_immediate_queue() {
    if (config.mode == TracingMode::Immediate) {
        async_queue().flush_now();
    }
}

inline void start_async_immediate(FILE* out = nullptr) {
    if (config.mode == TracingMode::Immediate) {
        async_queue().start(out);
    }
}

inline void stop_async_immediate() {
    if (config.mode == TracingMode::Immediate) {
        async_queue().stop();
    }
}

inline void ensure_stats_registered() {
    if (!stats_registered) {
        stats_registered = true;
        // Register atexit handler for stats
        std::atexit([]() {
            if (config.mode != TracingMode::Disabled) {
                dump_stats();
            }
        });
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
            num_buffers = 1;
        }
#endif
        
        for (int buf = 0; buf < num_buffers; ++buf) {
            const Event* events = r->buffers[buf];
            uint32_t count = r->counts[buf];
            uint32_t head = r->heads[buf];
            
            if (count == 0) continue;
            
            // Write thread info
            w32(r->thread_id);
            ws(r->thread_name.c_str(), static_cast<uint16_t>(r->thread_name.length()));
            w8(0); // null terminator
            
            // Write events
            w32(count);
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t idx = (head - count + i + TRC_RING_CAP) % TRC_RING_CAP;
                const Event& e = events[idx];
                
                w8(static_cast<uint8_t>(e.type));
                w64(e.timestamp);
                w32(e.depth);
                w32(e.color_offset);
                ws(e.function, static_cast<uint16_t>(std::strlen(e.function)));
                ws(e.file, static_cast<uint16_t>(std::strlen(e.file)));
                w32(e.line);
                ws(e.message, static_cast<uint16_t>(std::strlen(e.message)));
            }
        }
    }
    
    std::fclose(f);
    return filename;
}

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

namespace dll_shared_state {
    // Shared state structure
    struct SharedTraceState {
        uint32_t magic;
        uint32_t version;
        Config* config_ptr;
        Registry* registry_ptr;
        char process_name[64];
    };
    
    // Get or create shared state (thread-safe)
    inline SharedTraceState* get_shared_state() {
        static std::mutex init_mutex;
        static SharedTraceState* state = nullptr;
        static shared_memory::SharedMemoryHandle shm_handle;
        
        if (state) return state;
        
        std::lock_guard<std::mutex> lock(init_mutex);
        if (state) return state;  // Double-check
        
        // Try to open existing shared memory first (DLL case)
        std::string shm_name = shared_memory::get_shared_memory_name();
        shm_handle = shared_memory::create_or_open_shared_memory(
            shm_name.c_str(),
            sizeof(SharedTraceState),
            false  // Try open first
        );
        
        if (!shm_handle.valid) {
            // Doesn't exist, we might be the first/main EXE
            // This is OK - will be created by TRC_SETUP_DLL_SHARED()
            return nullptr;
        }
        
        // Access shared memory
        state = static_cast<SharedTraceState*>(shared_memory::get_mapped_address(shm_handle));
        
        // Validate magic number
        if (state->magic != 0x54524143) {  // "TRAC"
            state = nullptr;  // Invalid shared memory
        }
        
        return state;
    }
    
    inline Config* get_shared_config() {
        SharedTraceState* state = get_shared_state();
        return state ? state->config_ptr : nullptr;
    }
    
    inline Registry* get_shared_registry() {
        SharedTraceState* state = get_shared_state();
        return state ? state->registry_ptr : nullptr;
    }
    
    inline void set_shared_config(Config* cfg) {
        SharedTraceState* state = get_shared_state();
        if (state) state->config_ptr = cfg;
    }
    
    inline void set_shared_registry(Registry* reg) {
        SharedTraceState* state = get_shared_state();
        if (state) state->registry_ptr = reg;
    }
}

} // namespace trace

#endif // TRACE_SCOPE_FUNCTIONS_HPP
