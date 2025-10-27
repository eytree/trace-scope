#ifndef REL_OPS_HPP
#define REL_OPS_HPP

/**
 * @file rel_ops.hpp
 * @brief namespace rel_ops
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

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
} // namespace trace


#endif // REL_OPS_HPP
