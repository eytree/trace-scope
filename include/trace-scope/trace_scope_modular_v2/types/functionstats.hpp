#ifndef FUNCTIONSTATS_HPP
#define FUNCTIONSTATS_HPP

/**
 * @file FunctionStats.hpp
 * @brief FunctionStats struct definition
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

struct FunctionStats {
    const char* func_name;     ///< Function name
    uint64_t    call_count;   ///< Number of times function was called
    uint64_t    total_ns;     ///< Total execution time in nanoseconds
    uint64_t    min_ns;       ///< Minimum execution time in nanoseconds
    uint64_t    max_ns;       ///< Maximum execution time in nanoseconds
    uint64_t    memory_delta; ///< Memory delta in bytes (peak - start RSS)
    
    double avg_ns() const { return call_count > 0 ? (double)total_ns / call_count : 0.0; }
}

} // namespace trace


#endif // FUNCTIONSTATS_HPP
