#ifndef TRACE_SCOPE_SCOPE_HPP
#define TRACE_SCOPE_SCOPE_HPP

/**
 * @file Scope.hpp
 * @brief Scope struct definition for RAII function tracing
 */

#include <cstdint>

namespace trace {

// Forward declarations
enum class EventType : uint8_t;
enum class FlushMode;
struct Ring;
struct Config;

// Forward declarations for functions
inline Ring& thread_ring();
inline Config& get_config();
inline void flush_all();

/**
 * @brief RAII scope guard for automatic function entry/exit tracing.
 * 
 * Constructs on function entry, writes Enter event, then on destruction
 * writes Exit event with calculated duration. Flush behavior is controlled
 * by the flush_mode configuration setting.
 */
struct Scope {
    const char* func;  ///< Function name
    const char* file;  ///< Source file
    int         line;  ///< Source line
    
    /**
     * @brief Construct a scope guard and write Enter event.
     * @param f Function name
     * @param fi Source file
     * @param li Source line
     */
    inline Scope(const char* f, const char* fi, int li) : func(f), file(fi), line(li) {
#if TRC_ENABLED
        thread_ring().write(EventType::Enter, func, file, line);
#endif
    }
    
    /**
     * @brief Destruct the scope guard and write Exit event.
     * 
     * Writes Exit event with calculated duration. Flush behavior is
     * controlled by the flush_mode configuration setting.
     */
    inline ~Scope() {
#if TRC_ENABLED
        Ring& r = thread_ring();
        r.write(EventType::Exit, func, file, line);
        
        Config& cfg = get_config();
        if (cfg.flush_mode == FlushMode::EVERY_SCOPE) {
            flush_all();
        } else if (cfg.flush_mode == FlushMode::OUTERMOST_ONLY && r.depth == 0) {
            flush_all();
        }
#endif
    }
};

} // namespace trace

#endif // TRACE_SCOPE_SCOPE_HPP



