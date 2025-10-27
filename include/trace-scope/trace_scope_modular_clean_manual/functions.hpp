#ifndef FUNCTIONS_HPP
#define FUNCTIONS_HPP

/**
 * @file functions.hpp
 * @brief Function implementations
 */

namespace trace {

// Placeholder implementations - would need to be filled from original header
inline void set_external_state(Config* cfg, Registry* reg) {
    // Implementation from original header
}

inline bool load_config(const std::string& path) {
    return get_config().load_from_file(path);
}

inline void dump_stats() {
    // Implementation from original header
}

inline void filter_include_function(const std::string& func) {
    get_config().filter.include_functions.push_back(func);
}

inline void filter_exclude_function(const std::string& func) {
    get_config().filter.exclude_functions.push_back(func);
}

inline void filter_include_file(const std::string& file) {
    get_config().filter.include_files.push_back(file);
}

inline void filter_exclude_file(const std::string& file) {
    get_config().filter.exclude_files.push_back(file);
}

inline void filter_set_max_depth(uint32_t depth) {
    get_config().filter.max_depth = depth;
}

inline void filter_clear() {
    get_config().filter.include_functions.clear();
    get_config().filter.exclude_functions.clear();
    get_config().filter.include_files.clear();
    get_config().filter.exclude_files.clear();
}

inline void flush_immediate_queue() {
    async_queue().flush_now();
}

inline void start_async_immediate() {
    async_queue().start();
}

inline void stop_async_immediate() {
    async_queue().stop();
}

inline void ensure_stats_registered() {
    if (!stats_registered.load()) {
        stats_registered.store(true);
    }
}

inline std::string generate_dump_filename(const char* prefix = nullptr) {
    return "trace_dump.bin";
}

inline std::string dump_binary(const char* prefix = nullptr) {
    return "trace_dump.bin";
}

} // namespace trace

#endif // FUNCTIONS_HPP