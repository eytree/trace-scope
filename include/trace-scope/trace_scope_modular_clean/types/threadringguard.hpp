#ifndef THREADRINGGUARD_HPP
#define THREADRINGGUARD_HPP

/**
 * @file threadringguard.hpp
 * @brief ThreadRingGuard struct definition
 */

struct ThreadRingGuard {
    ~ThreadRingGuard() {
        if (dll_shared_state::get_shared_registry()) {
            // In DLL mode, remove this thread's Ring from registry
            dll_shared_state::get_shared_registry()->remove_thread_ring(std::this_thread::get_id());
        }
    }
}

#endif // THREADRINGGUARD_HPP
