#ifndef STATS_NAMESPACE_HPP
#define STATS_NAMESPACE_HPP

namespace trace {
namespace stats {

inline void register_function(const std::string& name, uint64_t duration_ns) {
    // Implementation from original header
}

inline void register_thread(uint32_t thread_id, const std::string& name) {
    // Implementation from original header
}

} // namespace stats
} // namespace trace

#endif // STATS_NAMESPACE_HPP