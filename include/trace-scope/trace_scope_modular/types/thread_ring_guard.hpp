#ifndef THREAD_RING_GUARD_HPP
#define THREAD_RING_GUARD_HPP

/**
 * @file thread_ring_guard.hpp
 * @brief ThreadRingGuard struct for DLL shared mode cleanup
 */

#include <thread>

namespace trace {

// Forward declarations
namespace dll_shared_state {
    class Registry;
    Registry* get_shared_registry();
}

/**
 * @brief RAII guard for thread-local ring cleanup in DLL shared mode.
 * 
 * Automatically removes the thread's ring from the shared registry
 * when the thread exits.
 */
struct ThreadRingGuard {
    ~ThreadRingGuard() {
        if (dll_shared_state::get_shared_registry()) {
            dll_shared_state::get_shared_registry()->remove_thread_ring(std::this_thread::get_id());
        }
    }
};

} // namespace trace

#endif // THREAD_RING_GUARD_HPP
