#ifndef STATS_HPP
#define STATS_HPP

/**
 * @file stats.hpp
 * @brief namespace stats
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

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
        int num_buffers = TRC_NUM_BUFFERS;
#if TRC_DOUBLE_BUFFER
        if (!get_config().use_double_buffering) {
            num_buffers = 1;  // Only process first buffer if not using double-buffering
        }
#endif
        
        for (int buf_idx = 0; buf_idx < num_buffers; ++buf_idx) {
            uint32_t count = (r->wraps[buf_idx] == 0) ? r->head[buf_idx] : TRC_RING_CAP;
            uint32_t start = (r->wraps[buf_idx] == 0) ? 0 : r->head[buf_idx];
            
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t idx = (start + i) % TRC_RING_CAP;
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

}
} // namespace trace


#endif // STATS_HPP
