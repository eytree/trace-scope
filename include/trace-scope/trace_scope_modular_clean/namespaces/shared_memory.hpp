#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

/**
 * @file shared_memory.hpp
 * @brief namespace shared_memory
 */

namespace shared_memory {
    // Platform-specific shared memory handle
    struct SharedMemoryHandle {
#ifdef _WIN32
        void* handle;
        void* mapped_view;
#else
        int fd;
        void* mapped_addr;
#endif
        bool valid;
    };
    
    // Create or open shared memory region
    inline SharedMemoryHandle create_or_open_shared_memory(const char* name, size_t size, bool create) {
        SharedMemoryHandle handle = {0};
        
#ifdef _WIN32
        handle.handle = nullptr;
        handle.mapped_view = nullptr;
        handle.valid = false;
        
        if (create) {
            handle.handle = CreateFileMappingA(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                0,
                static_cast<DWORD>(size),
                name
            );
        } else {
            handle.handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
        }
        
        if (handle.handle) {
            handle.mapped_view = MapViewOfFile(handle.handle, FILE_MAP_ALL_ACCESS, 0, 0, size);
            handle.valid = (handle.mapped_view != nullptr);
        }
#else
        handle.fd = -1;
        handle.mapped_addr = nullptr;
        handle.valid = false;
        
#ifdef __linux__ || defined(__APPLE__)
        int flags = O_RDWR;
        if (create) flags |= O_CREAT | O_EXCL;
        
        handle.fd = shm_open(name, flags, 0666);
        
        if (handle.fd >= 0) {
            if (create) {
                ftruncate(handle.fd, size);
            }
            handle.mapped_addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, handle.fd, 0);
            handle.valid = (handle.mapped_addr != MAP_FAILED);
        }
#else
        // POSIX shared memory not available on this platform
        handle.valid = false;
#endif
#endif
        
        return handle;
    }
    
    // Get the mapped address from a handle (platform-agnostic)
    inline void* get_mapped_address(const SharedMemoryHandle& handle) {
#ifdef _WIN32
        return handle.mapped_view;
#else
        return handle.mapped_addr;
#endif
    }
    
    // Close shared memory
    inline void close_shared_memory(SharedMemoryHandle& handle) {
        if (!handle.valid) return;
        
#ifdef _WIN32
        if (handle.mapped_view) {
            UnmapViewOfFile(handle.mapped_view);
            handle.mapped_view = nullptr;
        }
        if (handle.handle) {
            CloseHandle(handle.handle);
            handle.handle = nullptr;
        }
#else
#ifdef __linux__ || defined(__APPLE__)
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

#endif // SHARED_MEMORY_HPP
