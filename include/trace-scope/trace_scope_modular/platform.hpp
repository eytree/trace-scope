#ifndef TRACE_SCOPE_PLATFORM_HPP
#define TRACE_SCOPE_PLATFORM_HPP

/**
 * @file platform.hpp
 * @brief Platform-specific includes and build-time defines
 */

// Standard C++ includes
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <mutex>
#include <thread>
#include <algorithm>
#include <sstream>
#include <map>
#include <string>
#include <atomic>
#include <filesystem>

// Platform-specific includes for memory sampling
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
// Undefine Windows macros that conflict with std::min/max
#undef min
#undef max
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/task.h>
#endif

// Platform-specific includes for shared memory
#ifdef _WIN32
// Windows shared memory functions are in windows.h (already included)
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// DLL export/import macros for shared state across DLL boundaries
#ifndef TRC_SCOPE_API
      #define TRC_SCOPE_API
#endif

// Build-time configuration defines
#ifndef TRC_ENABLED
#define TRC_ENABLED 1
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
#define TRC_SCOPE_VERSION "0.14.1-alpha"
#define TRC_SCOPE_VERSION_MAJOR 0
#define TRC_SCOPE_VERSION_MINOR 14
#define TRC_SCOPE_VERSION_PATCH 1

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

// Cross-platform safe tmpfile wrapper
inline FILE* safe_tmpfile() {
#ifdef _MSC_VER
    FILE* file = nullptr;
    return tmpfile_s(&file) == 0 ? file : nullptr;
#else
    return std::tmpfile();
#endif
}

} // namespace trace

#endif // TRACE_SCOPE_PLATFORM_HPP
