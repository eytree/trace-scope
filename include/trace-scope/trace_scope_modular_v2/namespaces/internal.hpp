#ifndef INTERNAL_HPP
#define INTERNAL_HPP

/**
 * @file internal.hpp
 * @brief namespace internal
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

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

}
} // namespace trace


#endif // INTERNAL_HPP
