#ifndef SHARED_MEMORY_NAMESPACE_HPP
#define SHARED_MEMORY_NAMESPACE_HPP

namespace trace {
namespace shared_memory {

inline bool create_shared_memory(const std::string& name, size_t size) {
    // Implementation from original header
    return false;
}

inline void* map_shared_memory(const std::string& name) {
    // Implementation from original header
    return nullptr;
}

} // namespace shared_memory
} // namespace trace

#endif // SHARED_MEMORY_NAMESPACE_HPP