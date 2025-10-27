#ifndef THREADSTATS_HPP
#define THREADSTATS_HPP

/**
 * @file ThreadStats.hpp
 * @brief ThreadStats struct definition
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

struct ThreadStats {
    uint32_t tid;                                    ///< Thread ID
    std::vector<FunctionStats> functions;            ///< Function statistics for this thread
    uint64_t total_events;                          ///< Total events in this thread
    uint64_t peak_rss;                              ///< Peak RSS memory usage for this thread
}

} // namespace trace


#endif // THREADSTATS_HPP
