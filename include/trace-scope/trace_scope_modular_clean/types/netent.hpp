#ifndef NETENT_HPP
#define NETENT_HPP

/**
 * @file netent.hpp
 * @brief netent struct definition
 */

 */
enum class TracingMode {
    Buffered,   ///< Default: events buffered in ring buffer, manual flush required
    Immediate,  ///< Real-time output: bypass ring buffer, print immediately
    Hybrid      ///< Hybrid: buffer events AND print immediately for real-time + history
}

#endif // NETENT_HPP
