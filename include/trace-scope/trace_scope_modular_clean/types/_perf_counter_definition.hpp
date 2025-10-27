#ifndef _PERF_COUNTER_DEFINITION_HPP
#define _PERF_COUNTER_DEFINITION_HPP

/**
 * @file _perf_counter_definition.hpp
 * @brief _PERF_COUNTER_DEFINITION struct definition
 */

if (handle.mapped_addr && handle.mapped_addr != MAP_FAILED) {
            munmap(handle.mapped_addr, sizeof(SharedTraceState));
            handle.mapped_addr = nullptr;
        }
        if (handle.fd >= 0) {
            close(handle.fd);
            handle.fd = -1;
        }
#endif
#endif
        handle.valid = false;
    }
    
    // Get unique shared memory name for this process
    inline std::string get_shared_memory_name() {
#ifdef _WIN32
        DWORD pid = GetCurrentProcessId();
        char name[128];
        std::snprintf(name, sizeof(name), "Local\\trace_scope_%lu", pid);
        return name;
#elif defined(__linux__) || defined(__APPLE__)
        pid_t pid = getpid();
        char name[128];
        std::snprintf(name, sizeof(name), "/trace_scope_%d", pid);
        return name;
#else
        // Fallback for unsupported platforms
        return "/trace_scope_fallback";
#endif
    }
}

/

#endif // _PERF_COUNTER_DEFINITION_HPP
