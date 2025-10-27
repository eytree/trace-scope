#ifndef STRING_VIEW_LITERALS_HPP
#define STRING_VIEW_LITERALS_HPP

/**
 * @file string_view_literals.hpp
 * @brief namespace string_view_literals
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

struct ThreadRingGuard {
    ~ThreadRingGuard() {
        if (dll_shared_state::get_shared_registry()) {
            // In DLL mode, remove this thread's Ring from registry
            dll_shared_state::get_shared_registry()->remove_thread_ring(std::this_thread::get_id());
        }
    }
};

/**
 * @brief Get the current thread's ring buffer.
 * 
 * In DLL shared mode (when external registry is set), uses centralized
 * heap-allocated Ring storage to ensure all DLLs access the same Ring per thread.
 * 
 * In header-only mode, uses thread_local storage (each DLL gets its own copy).
 * 
 * @return Reference to the current thread's Ring
 */
inline Ring& thread_ring() {
    // Check if we should use shared mode
    if (dll_shared_state::get_shared_registry() || should_use_shared_memory()) {
     
} // namespace trace


#endif // STRING_VIEW_LITERALS_HPP
