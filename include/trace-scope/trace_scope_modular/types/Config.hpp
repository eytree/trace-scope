#ifndef CONFIG_HPP
#define CONFIG_HPP

/**
 * @file config.hpp
 * @brief Config struct definition
 */

namespace trace {

struct Config {
    TracingMode mode = TracingMode::Disabled;
    FlushMode flush_mode = FlushMode::Auto;
    SharedMemoryMode shared_memory_mode = SharedMemoryMode::Auto;
    uint32_t max_depth = TRC_DEPTH_MAX;
    bool auto_flush = true;
    uint32_t flush_interval_ms = 1000;
    
    struct {
        std::vector<std::string> include_functions;
        std::vector<std::string> exclude_functions;
        std::vector<std::string> include_files;
        std::vector<std::string> exclude_files;
        uint32_t max_depth = TRC_DEPTH_MAX;
    } filter;
    
    std::string config_file;
    FILE* output_file = nullptr;
    
    void load_from_file(const std::string& path);
    void save_to_file(const std::string& path) const;
};

} // namespace trace

#endif // CONFIG_HPP
