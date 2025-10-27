#ifndef EVENT_HPP
#define EVENT_HPP

/**
 * @file event.hpp
 * @brief Event struct definition
 */

namespace trace {

struct Event {
    uint64_t timestamp;           ///< Timestamp in nanoseconds
    EventType type;               ///< Event type
    uint32_t depth;              ///< Call stack depth
    const char* functiontion;        ///< Function name
    const char* file;            ///< Source file
    uint32_t line;               ///< Source line
    char message[TRC_MSG_CAP];   ///< Message buffer
};

} // namespace trace

#endif // EVENT_HPP
