#ifndef _PERF_INSTANCE_DEFINITION_HPP
#define _PERF_INSTANCE_DEFINITION_HPP

/**
 * @file _perf_instance_definition.hpp
 * @brief _PERF_INSTANCE_DEFINITION struct definition
 */

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
 

#endif // _PERF_INSTANCE_DEFINITION_HPP
