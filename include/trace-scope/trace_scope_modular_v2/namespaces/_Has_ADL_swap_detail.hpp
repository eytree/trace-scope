#ifndef _HAS_ADL_SWAP_DETAIL_HPP
#define _HAS_ADL_SWAP_DETAIL_HPP

/**
 * @file _Has_ADL_swap_detail.hpp
 * @brief namespace _Has_ADL_swap_detail
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

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
 
} // namespace trace


#endif // _HAS_ADL_SWAP_DETAIL_HPP
