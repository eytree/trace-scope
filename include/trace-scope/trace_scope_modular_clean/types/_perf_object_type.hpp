#ifndef _PERF_OBJECT_TYPE_HPP
#define _PERF_OBJECT_TYPE_HPP

/**
 * @file _perf_object_type.hpp
 * @brief _PERF_OBJECT_TYPE struct definition
 */

TRC_ENABLED 1
#endif

#ifndef TRC_RING_CAP
#define TRC_RING_CAP 4096
#endif

#ifndef TRC_MSG_CAP
#define TRC_MSG_CAP 192
#endif

#ifndef TRC_DEPTH_MAX
#define TRC_DEPTH_MAX 512
#endif

#ifndef TRC_DOUBLE_BUFFER
#define TRC_DOUBLE_BUFFER 0  // Default: disabled to save memory (~1.2MB per thread)
#endif

#if TRC_DOUBLE_BUFFER
#define TRC_NUM_BUFFERS 2
#else
#define TRC_NUM_BUFFERS 1
#endif

// Version information - keep in sync with VERSION file at project root
#define TRC_SCOPE_VERSION "0.14.0-alpha"
#define TRC_SCOPE_VERSION_MAJOR 0
#define TRC_SCOPE_VERSION_MINOR 14
#define TRC_SCOPE_VERSION_PATCH 0

namespace trace {

// Cross-platform safe fopen wrapper to avoid deprecation warnings
inline FILE* safe_fopen(const char* filename, const char* mode) {
#ifdef _MSC_VER
    FILE* file = nullptr;
    if (fopen_s(&file, filename, mode) != 0) {
        return nullptr;
    }
    return file;
#else
    return std::fopen(filename, mode);
#endif
}

// Cross-platform safe tmpfile wrapper to avoid deprecation warnings
inline FILE* safe_tmpfile() {
#ifdef _MSC_VER
    FILE* file = nullptr;
    if (tmpfile_s(&file) != 0) {
        return nullptr;
    }
    return file;
#else
    return std::tmpfile();
#endif
}

// Forward declarations
struct Config;
struct AsyncQueue;
inline AsyncQueue& async_queue();

/**
 * @brief Tracing output mode.
 * 
 * Determines how trace events are captured and output:
 * - Buffered: Events stored in ring buffer, flushed manually (default, best performance)
 * - Immediate: Events printed immediately, no buffering (real-time, higher overhead)
 * - Hybrid: Events both buffered AND printed immediately (best of both worlds)
 */
enum class TracingMode {
    Buffered,   ///< Default: events buffered in ring buffer, manual flush required
    Immediate,  ///< Real-time output: bypass ring buffer, print immediately
    Hybrid      ///< Hybrid: buffer events AND print immediately for real-time + history
};

/**
 * @brief INI file parser utilities for configuration loading.
 * 
 * Simple, dependency-free INI parser supporting:
 * - Comments (# and ;)
 * - Sections [section_name]
 

#endif // _PERF_OBJECT_TYPE_HPP
