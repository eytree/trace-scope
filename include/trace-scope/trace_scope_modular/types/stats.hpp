#ifndef TRACE_SCOPE_STATS_HPP
#define TRACE_SCOPE_STATS_HPP

/**
 * @file stats.hpp
 * @brief Performance statistics struct definitions
 */

#include <cstdint>
#include <vector>

namespace trace {

/**
 * @brief Performance statistics for a single function.
 */
struct FunctionStats {
    const char* func_name;     ///< Function name
    uint64_t    call_count;   ///< Number of times function was called
    uint64_t    total_ns;     ///< Total execution time in nanoseconds
    uint64_t    min_ns;       ///< Minimum execution time in nanoseconds
    uint64_t    max_ns;       ///< Maximum execution time in nanoseconds
    uint64_t    memory_delta; ///< Memory delta in bytes (peak - start RSS)
    
    double avg_ns() const { return call_count > 0 ? (double)total_ns / call_count : 0.0; }
};

/**
 * @brief Per-thread performance statistics.
 */
struct ThreadStats {
    uint32_t tid;                                    ///< Thread ID
    std::vector<FunctionStats> functions;            ///< Function statistics for this thread
    uint64_t total_events;                          ///< Total events in this thread
    uint64_t peak_rss;                              ///< Peak RSS memory usage for this thread
};

} // namespace trace

#endif // TRACE_SCOPE_STATS_HPP



