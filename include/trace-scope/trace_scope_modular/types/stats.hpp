#ifndef STATS_HPP
#define STATS_HPP

/**
 * @file stats.hpp
 * @brief Stats struct definitions
 */

namespace trace {

struct FunctionStats {
    std::string name;
    uint64_t call_count = 0;
    uint64_t total_time_ns = 0;
    uint64_t min_time_ns = UINT64_MAX;
    uint64_t max_time_ns = 0;
};

struct ThreadStats {
    uint32_t thread_id = 0;
    std::string thread_name;
    uint64_t event_count = 0;
    uint64_t total_time_ns = 0;
    std::unordered_map<std::string, FunctionStats> functions;
};

} // namespace trace

#endif // STATS_HPP