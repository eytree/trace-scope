#ifndef DLL_SHARED_STATE_HPP
#define DLL_SHARED_STATE_HPP

/**
 * @file dll_shared_state.hpp
 * @brief namespace dll_shared_state
 */

namespace dll_shared_state {
    // Shared state structure
    struct SharedTraceState {
        uint32_t magic;
        uint32_t version;
        Config* config_ptr;
        Registry* registry_ptr;
        char process_name[64];
    };
    
    // Get or create shared state (thread-safe)
    inline SharedTraceState* get_shared_state() {
        static std::mutex init_mutex;
        static SharedTraceState* state = nullptr;
        static shared_memory::SharedMemoryHandle shm_handle;
        
        if (state) return state;
        
        std::lock_guard<std::mutex> lock(init_mutex);
        if (state) return state;  // Double-check
        
        // Try to open existing shared memory first (DLL case)
        std::string shm_name = shared_memory::get_shared_memory_name();
        shm_handle = shared_memory::create_or_open_shared_memory(
            shm_name.c_str(),
            sizeof(SharedTraceState),
            false  // Try open first
        );
        
        if (!shm_handle.valid) {
            // Doesn't exist, we might be the first/main EXE
            // This is OK - will be created by TRC_SETUP_DLL_SHARED()
            return nullptr;
        }
        
        // Access shared memory
        state = static_cast<SharedTraceState*>(shared_memory::get_mapped_address(shm_handle));
        
        // Validate magic number
        if (state->magic != 0x54524143) {  // "TRAC"
            state = nullptr;  // Invalid shared memory
        }
        
        return state;
    }
    
    inline Config* get_shared_config() {
        SharedTraceState* state = get_shared_state();
        return state ? state->config_ptr : nullptr;
    }
    
    inline Registry* get_shared_registry() {
        SharedTraceState* state = get_shared_state();
        return state ? state->registry_ptr : nullptr;
    }
    
    inline void set_shared_config(Config* cfg) {
        SharedTraceState* state = get_shared_state();
        if (state) state->config_ptr = cfg;
    }
    
    inline void set_shared_registry(Registry* reg) {
        SharedTraceState* state = get_shared_state();
        if (state) state->registry_ptr = reg;
    }
}

#endif // DLL_SHARED_STATE_HPP
