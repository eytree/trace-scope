#ifndef THREADRINGGUARD_HPP
#define THREADRINGGUARD_HPP

/**
 * @file ThreadRingGuard.hpp
 * @brief ThreadRingGuard struct definition
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
}

} // namespace trace


#endif // THREADRINGGUARD_HPP
