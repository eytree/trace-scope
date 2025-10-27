#ifndef VARIABLES_HPP
#define VARIABLES_HPP

/**
 * @file variables.hpp
 * @brief Global variable declarations
 */

namespace trace {

extern Config config;
extern std::atomic<bool> stats_registered;

inline Config& get_config() {
    return config;
}

inline Registry& registry() {
    static Registry reg;
    return reg;
}

inline AsyncQueue& async_queue() {
    static AsyncQueue queue;
    return queue;
}

} // namespace trace

#endif // VARIABLES_HPP
