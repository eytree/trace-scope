#ifndef STDEXT_HPP
#define STDEXT_HPP

/**
 * @file stdext.hpp
 * @brief namespace stdext
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

 * 
 * int main() {
 *     TRC_SETUP_DLL_SHARED();  // One line - automatic setup & cleanup!
 *     
 *     // Configure tracing
 *     trace::config.out = std::fopen("trace.log", "w");
 *     
 *     // Use tracing normally across all DLLs
 *     TRC_SCOPE();
 *     call_dll_functions();  // All DLLs share same trace state and Ring buffers
 *     
 *     return 0;  // Automatic flush on exit via RAII
 * }
 * @endcode
 * 
 * Benefits over manual setup:
 * - 1 line instead of 20+
 * - Automatic cleanup (no manual flush needed)
 * - Exception-safe (RAII guarantees cleanup)
 * - Less error-prone
 * - Proper Ring buffer sharing (fixed in v0.9.0-alpha)
 * 
 * Note: For advanced control, you can still use set_external_state() manually.
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

/**
 * @brief Get the active config instance.
 * 
 * Returns external config if set via set_external_state(),
 * otherwise returns the default inline instance.
 * 
 * @return Reference to the active Config
 */
inline Config& get_config() {
    return dll_shared_state::get_shared_config() ? *dll_shared_state::get_shared_config() : config;
}

/**
 * @brief Config::load_from_file() implementation.
 * 
 * Parses INI file and applies configuration settings.
 */
inline bool Config::load_from_file(const char* path) {
    FILE* f = safe_fopen(path, "r");
    if (!f) {
        std::fprintf(stderr, "trace-scope: Warning: Could not open config file: %s\n", path);
        return false;
    }
    
    std::string current_section;
    char line_buf[512];
    int line_num = 0;
    
    while (std::fgets(line_buf, sizeof(line_buf), f)) {
        ++line_num;
        std::string line = ini_parser::trim(line_buf);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Parse section header [section_name]
        if (line[0] == '[' && line[line.length()-1] == ']') {
            current_section = line.substr(1, line.length() - 2);
            current_section = ini_parser::trim(current_section);
            continue;
        }
        
        // Parse key = value
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            std::fprintf(stderr, "trace-scope: Warning: Invalid line in %s:%d (no '=')\n", path, line_num);
            continue;
        }
        
        std::string key = ini_parser::trim(line.substr(0, eq_pos));
        std::string value = ini_parser::trim(line.substr(eq_pos + 1));
        
        // Remove inline comments from value
        size_t comment_pos = value.find('#');
        if (comment_pos == std::string::npos) {
            comment_pos = value.find(';');
        }
        if (comment_pos != std::string::npos) {
            value = ini_parser::trim(value.substr(0, comment_pos));
        }
        
        // Apply configuration based on section and key
        if (current_section == "output") {
            if (key == "file") {
                FILE* new_out = safe_fopen(ini_parser::unquote(value).c_str(), "w");
                if (new_out) {
                    if (out && out != stdout) std::fclose(out);
                    out = new_out;
                }
            }
            else if (key == "immediate_file") {
                FILE* new_imm = safe_fopen(ini_parser::unquote(value).c_str(), "w");
                if (new_imm) {
                    if (immediate_out && immediate_out != stdout) std::fclose(immediate_out);
                    immediate_out = new_imm;
                }
            }
        }
        else if (current_section == "display") {
            if (key == "print_timing") print_timing = ini_parser::parse_bool(value);
            else if (key == "print_timestamp") print_timestamp = ini_parser::parse_bool(value);
            else if (key == "print_thread") print_thread = ini_parser::parse_bool(value);
            else if (key == "colorize_depth") colorize_depth = ini_parser::parse_bool(value);
            else if (key == "include_file_line") include_file_line = ini_parser::parse_bool(value);
            else if (key == "include_filename") include_filename = ini_parser::parse_bool(value);
            else if (key == "include_function_name") include_function_name = ini_parser::parse_bool(value);
            else if (key == "show_full_path") show_full_path = ini_parser::parse_bool(value);
        }
        else if (current_section == "performance") {
            if (key == "print_stats") print_stats = ini_parser::parse_bool(value);
            else if (key == "track_memory") track_memory = ini_parser::parse_bool(value);
        }
        else if (current_section == "dump") {
            if (key == "prefix") {
                static std::string prefix_str = ini_parser::unquote(value);
                dump_prefix = prefix_str.c_str();
            }
            else if (key == "suffix") {
                static std::string suffix_str = ini_parser::unquote(value);
                dump_suffix = suffix_str.c_str();
            }
            else if (key == "output_dir") {
                static std::string dir_str = ini_parser::unquote(value);
                output_dir = dir_str.c_str();
            }
            else if (key == "layout") {
                std::string layout_str = ini_parser::trim(value);
                // Convert to lowercase for case-insensitive comparison
                for (char& c : layout_str) {
                    c = (char)std::tolower((unsigned char)c);
                }
                if (layout_str == "flat") output_layout = OutputLayout::Flat;
                else if (layout_str == "date" || layout_str == "bydate") output_layout = OutputLayout::ByDate;
                else if (layout_str == "session" || layout_str == "bysession") output_layout = OutputLayout::BySession;
                else {
                    std::fprintf(stderr, "trace-scope: Warning: Unknown layout '%s' in %s:%d\n", 
                                value.c_str(), path, line_num);
                }
            }
            else if (key == "session") {
                current_session = ini_parser::parse_int(value);
            }
        }
        else if (current_section == "formatting") {
            if (key == "filename_width") filename_width = ini_parser::parse_int(value);
            else if (key == "line_width") line_width = ini_parser::parse_int(value);
            else if (key == "function_width") function_width = ini_parser::parse_int(value);
        }
        else if (current_section == "markers") {
            if (key == "show_indent_markers") show_indent_markers = ini_parser::parse_bool(value);
            else if (key == "indent_marker") {
                static std::string s_indent = ini_parser::unquote(value);
                indent_marker = s_indent.c_str();
            }
            else if (key == "enter_marker") {
                static std::string s_enter = ini_parser::unquote(value);
                enter_marker = s_enter.c_str();
            }
            else if (key == "exit_marker") {
                static std::string s_exit = ini_parser::unquote(value);
                exit_marker = s_exit.c_str();
            }
            else if (key == "message_marker") {
                static std::string s_msg = ini_parser::unquote(value);
                msg_marker = s_msg.c_str();
            }
        }
        else if (current_section == "modes") {
            if (key == "mode") {
                std::string m = ini_parser::trim(value);
                // Convert to lowercase for case-insensitive comparison
                for (char& c : m) {
                    c = (char)std::tolower((unsigned char)c);
                }
                if (m == "buffered") mode = TracingMode::Buffered;
                else if (m == "immediate") mode = TracingMode::Immediate;
                else if (m == "hybrid") mode = TracingMode::Hybrid;
                else {
                    std::fprintf(stderr, "trace-scope: Warning: Unknown mode '%s' in %s:%d\n", 
                                value.c_str(), path, line_num);
                }
            }
            else if (key == "auto_flush_at_exit") auto_flush_at_exit = ini_parser::parse_bool(value);
            else if (key == "use_double_buffering") use_double_buffering = ini_parser::parse_bool(value);
            else if (key == "auto_flush_threshold") auto_flush_threshold = ini_parser::parse_float(value);
            else if (key == "immediate_flush_interval_ms") immediate_flush_interval_ms = ini_parser::parse_int(value);
            else if (key == "immediate_queue_size") immediate_queue_size = (size_t)ini_parser::parse_int(value);
            else if (key == "flush_mode") {
                std::string mode_str = ini_parser::trim(value);
                // Convert to lowercase for case-insensitive comparison
                for (char& c : mode_str) {
                    c = (char)std::tolower((unsigned char)c);
                }
                if (mode_str == "never") flush_mode = FlushMode::NEVER;
                else if (mode_str == "outermost") flush_mode = FlushMode::OUTERMOST_ONLY;
                else if (mode_str == "every") flush_mode = FlushMode::EVERY_SCOPE;
                else {
                    std::fprintf(stderr, "trace-scope: Warning: Unknown flush_mode '%s' in %s:%d\n", 
                                value.c_str(), path, line_num);
                }
            }
            else if (key == "shared_memory_mode") {
                std::string mode_str = ini_parser::trim(value);
                // Convert to lowercase for case-insensitive comparison
                for (char& c : mode_str) {
                    c = (char)std::tolower((unsigned char)c);
                }
                if (mode_str == "auto") shared_memory_mode = SharedMemoryMode::AUTO;
                else if (mode_str == "disabled") shared_memory_mode = SharedMemoryMode::DISABLED;
                else if (mode_str == "enabled") shared_memory_mode = SharedMemoryMode::ENABLED;
                else {
                    std::fprintf(stderr, "trace-scope: Warning: Unknown shared_memory_mode '%s' in %s:%d\n", 
                                value.c_str(), path, line_num);
                }
            }
        }
        else if (current_section == "filter") {
            if (key == "include_function") {
                filter.include_functions.push_back(ini_parser::unquote(value));
            }
            else if (key == "exclude_function") {
                filter.exclude_functions.push_back(ini_parser::unquote(value));
            }
            else if (key == "include_file") {
                filter.include_files.push_back(ini_parser::unquote(value));
            }
            else if (key == "exclude_file") {
                filter.exclude_files.push_back(ini_parser::unquote(value));
            }
            else if (key == "max_depth") {
                filter.max_depth = ini_parser::parse_int(value);
            }
        }
    }
    
    std::fclose(f);
    return true;
}

/**
 * @brief Load configuration from file into global config.
 * 
 * Convenience function that calls get_config().load_from_file().
 * 
 * @param path Path to INI file
 * @return true on success, false if file not found
 * 
 * Example:
 * @code
 * trace::load_config("trace.conf");
 * TRC_SCOPE();  // Now configured from file
 * @endcode
 */
inline bool load_config(const char* path) {
    return get_config().load_from_file(path);
}

/**
 * @brief Implementation of should_trace() - must be after get_config() is available.
 */
inline bool filter_utils::should_trace(const char* func, const char* file, int depth) {
    const auto& f = get_config().filter;
    
    // Check depth filter
    if (f.max_depth >= 0 && depth > f.max_depth) {
        return false;
    }
    
    // Check function filters
    if (func) {
        // If exclude list matches, filter out (exclude wins)
        if (matches_any(func, f.exclude_functions)) {
            return false;
        }
        
        // If include list is not empty and doesn't match, filter out
        if (!f.include_functions.empty() && !matches_any(func, f.include_functions)) {
            return false;
        }
    }
    
    // Check file filters
    if (file) {
        // If exclude list matches, filter out (exclude wins)
        if (matches_any(file, f.exclude_files)) {
            return false;
        }
        
        // If include list is not empty and doesn't match, filter out
        if (!f.include_files.empty() && !matches_any(file, f.include_files)) {
            return false;
        }
    }
    
    return true;  // Passed all filters
}

/**
 * @brief Add function include pattern (wildcard supported).
 * 
 * Only functions matching include patterns will be traced (unless excluded).
 * Empty include list means include all.
 * 
 * @param pattern Wildcard pattern (e.g., "my_namespace::*", "core_*")
 */
inline void filter_include_function(const char* pattern) {
    get_config().filter.include_functions.push_back(pattern);
}

/**
 * @brief Add function exclude pattern (wildcard supported).
 * 
 * Functions matching exclude patterns will never be traced.
 * Exclude takes priority over include.
 * 
 * @param pattern Wildcard pattern (e.g., "*_test", "debug_*")
 */

} // namespace trace


#endif // STDEXT_HPP
