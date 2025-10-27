#ifndef TRACE_SCOPE_MACROS_HPP
#define TRACE_SCOPE_MACROS_HPP

/**
 * @file macros.hpp
 * @brief All TRC_* macro definitions for trace-scope
 */

#include <cstdarg>
#include <sstream>
#include <string>

namespace trace {

// Forward declarations
struct Scope;
inline void trace_msgf(const char* file, int line, const char* fmt, ...);
template<typename T>
inline void trace_arg(const char* file, int line, const char* name, const char* type, const T& value);
inline void trace_arg(const char* file, int line, const char* name, const char* type);
template<typename Container>
inline std::string format_container(const Container& c, size_t max_elem = 5);

/**
 * @brief Stream-based logging class for TRC_LOG macro.
 * 
 * Provides C++ iostream-style logging using operator<<. Drop-in replacement
 * for stream-based logging macros. The message is associated with the
 * current function and displayed at the current indentation depth.
 */
struct TraceStream {
    std::ostringstream ss;  ///< Stream buffer for collecting output
    const char* file;       ///< Source file path
    int line;               ///< Source line number
    
    /**
     * @brief Constructor.
     * @param f Source file path
     * @param l Source line number
     */
    TraceStream(const char* f, int l) : file(f), line(l) {}
    
    /**
     * @brief Destructor: Outputs collected message.
     */
    ~TraceStream() {
        trace_msgf(file, line, "%s", ss.str().c_str());
    }
    
    /**
     * @brief Stream insertion operator.
     * @tparam T Type of value to insert
     * @param val Value to insert
     * @return Reference to this stream
     */
    template<typename T>
    TraceStream& operator<<(const T& val) {
        ss << val;
        return *this;
    }
};

} // namespace trace

// ============================================================================
// Public API Macros
// ============================================================================

/**
 * @def TRC_SCOPE()
 * @brief Create a scope guard for automatic function entry/exit tracing.
 * 
 * Creates a RAII object that automatically logs function entry on construction
 * and function exit (with duration) on destruction. The scope guard tracks
 * call depth and provides automatic indentation in the output.
 * 
 * Example:
 * @code
 * void my_function() {
 *     TRC_SCOPE();
 *     // Function body - entry/exit automatically traced
 * }
 * @endcode
 */
#define TRC_SCOPE() ::trace::Scope _trace_scope_obj(__func__, __FILE__, __LINE__)

/**
 * @def TRC_MSG(fmt, ...)
 * @brief Log a formatted message within the current scope.
 * 
 * Uses printf-style format strings. The message is associated with the
 * current function and displayed at the current indentation depth.
 * 
 * Example:
 * @code
 * TRC_MSG("Processing item %d of %d", current, total);
 * @endcode
 * 
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
#define TRC_MSG(...) ::trace::trace_msgf(__FILE__, __LINE__, __VA_ARGS__)

/**
 * @def TRC_LOG
 * @brief Stream-based logging within the current scope.
 * 
 * Provides C++ iostream-style logging using operator<<. Drop-in replacement
 * for stream-based logging macros. The message is associated with the
 * current function and displayed at the current indentation depth.
 * 
 * Example:
 * @code
 * int id = 42;
 * std::string name = "test";
 * TRC_LOG << "Processing item " << id << ", name=" << name;
 * 
 * // Drop-in replacement for:
 * // KY_COUT("Processing item " << id << ", name=" << name);
 * @endcode
 */
#define TRC_LOG ::trace::TraceStream(__FILE__, __LINE__)

/**
 * @def TRC_CONTAINER(container, max_elements)
 * @brief Helper macro to format containers for TRC_ARG.
 * 
 * Shows up to max_elements from the container, then "..." if more exist.
 * Single-line output: [elem1, elem2, elem3, ...]
 * 
 * Example:
 * @code
 * std::vector<int> values = {1, 2, 3, 4, 5, 6, 7};
 * TRC_ARG("values", std::vector<int>, TRC_CONTAINER(values, 5));
 * // Output: values: std::vector<int> = [1, 2, 3, 4, 5, ...]
 * @endcode
 * 
 * @param container The container to format
 * @param max_elements Maximum number of elements to display
 */
#define TRC_CONTAINER(container, max_elements) ::trace::format_container(container, max_elements)

/**
 * @def TRC_ARG(name, type, ...)
 * @brief Log a function argument with its name, type, and optionally its value.
 * 
 * Used to automatically log function parameters. Can be used with or without
 * the value parameter. For printable types, include the value. For complex
 * types, omit the value.
 * 
 * Example:
 * @code
 * void process(int id, const std::vector<int>& values, MyClass& obj) {
 *     TRC_SCOPE();
 *     TRC_ARG("id", int, id);  // Printable type with value
 *     TRC_ARG("values", std::vector<int>, TRC_CONTAINER(values, 5));  // Container
 *     TRC_ARG("obj", MyClass);  // Non-printable type, no value
 * }
 * @endcode
 * 
 * @param name String literal with the parameter name
 * @param type Type of the parameter
 * @param ... Optional value or formatted value (for printable types)
 */
#define TRC_ARG(...) ::trace::trace_arg(__FILE__, __LINE__, __VA_ARGS__)

// ============================================================================
// DLL Setup Macros
// ============================================================================

/**
 * @def TRC_SETUP_DLL_SHARED_WITH_CONFIG(config_file)
 * @brief Set up DLL state sharing with optional config file.
 * 
 * Creates shared memory region and external state for cross-DLL tracing.
 * All DLLs and the main executable will share the same Config and Registry
 * instances, enabling unified tracing across process boundaries.
 * 
 * This macro should be called once in main() before any tracing occurs.
 * It creates a RAII guard that automatically handles cleanup on exit.
 * 
 * Example:
 * @code
 * int main() {
 *     TRC_SETUP_DLL_SHARED_WITH_CONFIG("trace.ini");
 *     // ... rest of application
 *     return 0;  // Automatic cleanup via RAII
 * }
 * @endcode
 * 
 * @param config_file Path to INI config file (nullptr for no config)
 */
#define TRC_SETUP_DLL_SHARED_WITH_CONFIG(config_file) \
    static trace::Config g_trace_shared_config; \
    static trace::Registry g_trace_shared_registry; \
    static trace::shared_memory::SharedMemoryHandle g_shm_handle; \
    static trace::dll_shared_state::SharedTraceState* g_shared_state = nullptr; \
    static struct TraceDllGuard { \
        TraceDllGuard() { \
            /* Create shared memory */ \
            std::string shm_name = trace::shared_memory::get_shared_memory_name(); \
            g_shm_handle = trace::shared_memory::create_or_open_shared_memory( \
                shm_name.c_str(), \
                sizeof(trace::dll_shared_state::SharedTraceState), \
                true /* create */ \
            ); \
            \
            if (g_shm_handle.valid) { \
                /* Initialize shared state */ \
                g_shared_state = static_cast<trace::dll_shared_state::SharedTraceState*>( \
                    trace::shared_memory::get_mapped_address(g_shm_handle)); \
                g_shared_state->magic = 0x54524143; \
                g_shared_state->version = 1; \
                g_shared_state->config_ptr = &g_trace_shared_config; \
                g_shared_state->registry_ptr = &g_trace_shared_registry; \
                std::strncpy(g_shared_state->process_name, "trace-scope", 63); \
                g_shared_state->process_name[63] = '\0'; \
            } \
            \
            trace::set_external_state(&g_trace_shared_config, &g_trace_shared_registry); \
            const char* cfg_path = (config_file); \
            if (cfg_path && cfg_path[0]) { \
                g_trace_shared_config.load_from_file(cfg_path); \
            } \
        } \
        ~TraceDllGuard() { \
            trace::flush_all(); \
            if (g_shm_handle.valid) { \
                trace::shared_memory::close_shared_memory(g_shm_handle); \
            } \
        } \
    } g_trace_dll_guard;

/**
 * @def TRC_SETUP_DLL_SHARED()
 * @brief DLL state sharing setup without config file (backward compatible).
 * 
 * Same as TRC_SETUP_DLL_SHARED_WITH_CONFIG(nullptr).
 */
#define TRC_SETUP_DLL_SHARED() TRC_SETUP_DLL_SHARED_WITH_CONFIG(nullptr)

#endif // TRACE_SCOPE_MACROS_HPP

