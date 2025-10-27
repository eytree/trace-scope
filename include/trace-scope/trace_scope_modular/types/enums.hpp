#ifndef ENUMS_HPP
#define ENUMS_HPP

/**
 * @file enums.hpp
 * @brief Enum definitions
 */

namespace trace {

enum class EventType : uint8_t {
    Enter = 0,
    Exit = 1,
    Message = 2,
    Marker = 3
};

enum class TracingMode : uint8_t {
    Disabled = 0,
    Immediate = 1,
    Buffered = 2,
    Hybrid = 3
};

enum class FlushMode : uint8_t {
    Manual = 0,
    Auto = 1,
    Interval = 2
};

enum class SharedMemoryMode : uint8_t {
    Disabled = 0,
    Auto = 1,
    Enabled = 2
};

} // namespace trace

#endif // ENUMS_HPP