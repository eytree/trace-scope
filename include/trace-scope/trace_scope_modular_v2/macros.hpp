#ifndef MACROS_HPP
#define MACROS_HPP

/**
 * @file macros.hpp
 * @brief TRC_* macro definitions
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

#define TRC_NUM_BUFFERS 2
#define TRC_NUM_BUFFERS 1
#define TRC_SCOPE_VERSION "0.14.0-alpha"
#define TRC_SCOPE_VERSION_MAJOR 0
#define TRC_SCOPE_VERSION_MINOR 14
#define TRC_SCOPE_VERSION_PATCH 0
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
#define TRC_SETUP_DLL_SHARED() TRC_SETUP_DLL_SHARED_WITH_CONFIG(nullptr)
#define TRC_SCOPE() ::trace::Scope _trace_scope_obj(__func__, __FILE__, __LINE__)
#define TRC_MSG(...) ::trace::trace_msgf(__FILE__, __LINE__, __VA_ARGS__)
#define TRC_LOG ::trace::TraceStream(__FILE__, __LINE__)
#define TRC_CONTAINER(container, max_elements) ::trace::format_container(container, max_elements)
#define TRC_ARG(...) ::trace::trace_arg(__FILE__, __LINE__, __VA_ARGS__)

#endif // MACROS_HPP
