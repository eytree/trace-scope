#ifndef REGISTRY_HPP
#define REGISTRY_HPP

/**
 * @file registry.hpp
 * @brief Registry struct definition
 */

namespace trace {

struct Registry {
    std::unordered_map<uint32_t, std::unique_ptr<Ring>> rings;
    std::mutex mutex;
    
    Ring* get_ring(uint32_t thread_id);
    void flush_all();
    void clear();
};

} // namespace trace

#endif // REGISTRY_HPP
