#ifndef RING_HPP
#define RING_HPP

/**
 * @file ring.hpp
 * @brief Ring struct definition
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
    inline bool should_auto_flush() const {
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
}

#endif // RING_HPP
