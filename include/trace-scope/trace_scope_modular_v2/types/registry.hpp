#ifndef REGISTRY_HPP
#define REGISTRY_HPP

/**
 * @file Registry.hpp
 * @brief Registry struct definition
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

struct Registry {
    std::mutex mtx;                 ///< Protects rings vector and thread_rings map
    std::vector<Ring*> rings;       ///< Pointers to all registered ring buffers
    std::map<std::thread::id, Ring*> thread_rings;  ///< Thread ID to Ring mapping for DLL sharing

    /**
     * @brief Register a new ring buffer.
     * @param r Pointer to ring buffer (must remain valid)
     */
    inline void add(Ring* r) {
        std::lock_guard<std::mutex> lock(mtx);
        rings.push_back(r);
    }
    
    /**
     * @brief Unregister a ring buffer (called from Ring destructor).
     * @param r Pointer to ring buffer to remove
     */
    inline void remove(Ring* r) {
        std::lock_guard<std::mutex> lock(mtx);
        rings.erase(std::remove(rings.begin(), rings.end(), r), rings.end());
    }
    
    /**
     * @brief Get or create Ring for current thread (DLL shared mode).
     * 
     * In DLL shared mode, Rings are heap-allocated and managed by the Registry
     * to ensure all DLLs access the same Ring per thread.
     * 
     * @return Pointer to Ring for current thread (never null)
     */
    inline Ring* get_or_create_thread_ring() {
        std::thread::id tid = std::this_thread::get_id();
        
        std::lock_guard<std::mutex> lock(mtx);
        
        // Check if Ring already exists for this thread
        auto it = thread_rings.find(tid);
        if (it != thread_rings.end()) {
            return it->second;
        }
        
        // Create new Ring on heap
        Ring* ring = new Ring();
        thread_rings[tid] = ring;
        rings.push_back(ring);  // Also add to flush list
        ring->registered = true;
        
        return ring;
    }
    
    /**
     * @brief Remove Ring for specific thread (DLL shared mode cleanup).
     * 
     * Called when a thread exits in DLL shared mode. Removes the Ring from
     * both the thread_rings map and the rings vector, then deletes it.
     * 
     * @param tid Thread ID to remove
     */
    inline void remove_thread_ring(std::thread::id tid) {
        std::lock_guard<std::mutex> lock(mtx);
        
        auto it = thread_rings.find(tid);
        if (it != thread_rings.end()) {
            Ring* ring = it->second;
            
            // Remove from both collections
            rings.erase(std::remove(rings.begin(), rings.end(), ring), rings.end());
            thread_rings.erase(it);
            
            // Delete the heap-allocated Ring
            delete ring;
        }
    }
}

} // namespace trace


#endif // REGISTRY_HPP
