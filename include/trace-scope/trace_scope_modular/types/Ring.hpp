#ifndef RING_HPP
#define RING_HPP

/**
 * @file ring.hpp
 * @brief Ring struct definition
 */

namespace trace {

struct Ring {
    Event buffers[TRC_NUM_BUFFERS][TRC_RING_CAP];  ///< Event buffers
    uint32_t counts[TRC_NUM_BUFFERS] = {0};       ///< Event counts per buffer
    uint32_t heads[TRC_NUM_BUFFERS] = {0};         ///< Next write position per buffer
    uint32_t depth = 0;                            ///< Current call depth
    uint32_t thread_id = 0;                        ///< Thread ID
    std::string thread_name;                       ///< Thread name
    
    Ring();
    ~Ring();
    
    bool write(const Event& event);
    bool write_msg(const char* msg, ...);
    bool should_auto_flush() const;
};

} // namespace trace

#endif // RING_HPP
