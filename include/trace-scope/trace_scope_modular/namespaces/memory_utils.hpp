#ifndef MEMORY_UTILS_HPP
#define MEMORY_UTILS_HPP

/**
 * @file memory_utils.hpp
 * @brief namespace memory_utils
 */

namespace memory_utils {

/**
 * @brief Get current process RSS (Resident Set Size) in bytes.
 * 
 * Cross-platform implementation:
 * - Windows: Uses GetProcessMemoryInfo()
 * - Linux: Parses /proc/self/status for VmRSS
 * - macOS: Uses task_info() with MACH_TASK_BASIC_INFO
 * 
 * @return RSS in bytes, or 0 if unable to determine
 */
inline uint64_t get_current_rss() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#elif defined(__linux__)
    FILE* f = fopen("/proc/self/status", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                uint64_t rss_kb;
                if (sscanf(line + 6, "%lu", &rss_kb) == 1) {
                    fclose(f);
                    return rss_kb * 1024;  // Convert KB to bytes
                }
            }
        }
        fclose(f);
    }
#elif defined(__APPLE__)
    struct task_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS) {
        return info.resident_size;
    }
#endif
    return 0;
}

}

#endif // MEMORY_UTILS_HPP
