#ifndef _PERF_DATA_BLOCK_HPP
#define _PERF_DATA_BLOCK_HPP

/**
 * @file _perf_data_block.hpp
 * @brief _PERF_DATA_BLOCK struct definition
 */

 <chrono>
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
#

#endif // _PERF_DATA_BLOCK_HPP
