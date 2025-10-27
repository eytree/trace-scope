#ifndef MACROS_HPP
#define MACROS_HPP

/**
 * @file macros.hpp
 * @brief TRC_* macro definitions
 */

// DLL setup macros
#define TRC_SETUP_DLL_SHARED_WITH_CONFIG(config_file) \
    static trace::Config g_trace_shared_config; \
    static trace::Registry g_trace_shared_registry; \
    static trace::Scope g_trace_setup_scope(__func__, __FILE__, __LINE__)

#define TRC_SETUP_DLL_SHARED() TRC_SETUP_DLL_SHARED_WITH_CONFIG(nullptr)

// Main tracing macros
#define TRC_SCOPE() ::trace::Scope _trace_scope_obj(__func__, __FILE__, __LINE__)

#define TRC_MSG(...) ::trace::trace_msgf(__FILE__, __LINE__, __VA_ARGS__)

#define TRC_LOG ::trace::TraceStream(__FILE__, __LINE__)

#define TRC_CONTAINER(container, max_elements) ::trace::format_container(container, max_elements)

#define TRC_ARG(...) ::trace::trace_arg(__FILE__, __LINE__, __VA_ARGS__)

#endif // MACROS_HPP