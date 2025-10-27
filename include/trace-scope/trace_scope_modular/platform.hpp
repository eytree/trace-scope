#ifndef PLATFORM_HPP
#define PLATFORM_HPP

/**
 * @file platform.hpp
 * @brief Platform-specific includes and build-time defines
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <mach/mach.h>
#include <mach/task.h>
#include <map>
#include <mutex>
#include <psapi.h>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <windows.h>

#define TRC_NUM_BUFFERS 2
#define TRC_NUM_BUFFERS 1
#define TRC_SCOPE_VERSION "0.14.0-alpha"
#define TRC_SCOPE_VERSION_MAJOR 0
#define TRC_SCOPE_VERSION_MINOR 14
#define TRC_SCOPE_VERSION_PATCH 0
#define TRC_SETUP_DLL_SHARED_WITH_CONFIG(config_file) \
#define TRC_SETUP_DLL_SHARED() TRC_SETUP_DLL_SHARED_WITH_CONFIG(nullptr)
#define TRC_SCOPE() ::trace::Scope _trace_scope_obj(__func__, __FILE__, __LINE__)
#define TRC_MSG(...) ::trace::trace_msgf(__FILE__, __LINE__, __VA_ARGS__)
#define TRC_LOG ::trace::TraceStream(__FILE__, __LINE__)
#define TRC_CONTAINER(container, max_elements) ::trace::format_container(container, max_elements)
#define TRC_ARG(...) ::trace::trace_arg(__FILE__, __LINE__, __VA_ARGS__)

#endif // PLATFORM_HPP
