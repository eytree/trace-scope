#ifndef CONFIG_HPP
#define CONFIG_HPP

/**
 * @file Config.hpp
 * @brief Config struct definition
 * 
 * Generated from trace_scope_original.hpp using AST extraction
 */

namespace trace {

struct Config {
    FILE* out = stdout;               ///< Output file stream (default: stdout)
    bool print_timing = true;         ///< Show function durations with auto-scaled units
    bool print_timestamp = false;     ///< Show ISO timestamps [YYYY-MM-DD HH:MM:SS.mmm] (opt-in)
    bool print_thread = true;         ///< Show thread ID in hex format
    bool auto_flush_at_exit = false;  ///< Automatically flush when outermost scope exits (opt-in)
    
    // Tracing mode
    TracingMode mode = TracingMode::Buffered;  ///< Tracing output mode (default: Buffered)
    FILE* immediate_out = nullptr;    ///< Separate output stream for immediate output in Hybrid mode (nullptr = use 'out')
    float auto_flush_threshold = 0.9f; ///< Auto-flush when buffer reaches this fraction full in Hybrid mode (0.0-1.0, default 0.9 = 90%)
    
    // Async immediate mode configuration
    int immediate_flush_interval_ms = 1;  ///< Flush interval for async immediate mode (default: 1ms, 0 = flush every event)
    size_t immediate_queue_size = 128;    ///< Max queue size hint for async immediate mode (default: 128)

    // Prefix content control
    bool include_file_line = true;    ///< Include filename:line in prefix block

    // Filename rendering options
    bool include_filename = true;     ///< Show filename in prefix
    bool show_full_path = false;      ///< Show full path vs basename only
    int  filename_width = 20;         ///< Fixed width for filename column
    int  line_width     = 5;          ///< Fixed width for line number
    
    // Function name rendering options
    bool include_function_name = true;  ///< Show function name in prefix (line number pairs with this)
    int  function_width = 20;           ///< Fixed width for function name column
    
    // Indentation and marker visualization
    bool show_indent_markers = true;    ///< Show visual markers for indentation levels
    const char* indent_marker = "| ";   ///< Marker for each indentation level (e.g., "| ", "  ", "│ ")
    const char* enter_marker = "-> ";   ///< Marker for function entry (e.g., "-> ", "↘ ", "► ")
    const char* exit_marker = "<- ";    ///< Marker for function exit (e.g., "<- ", "↖ ", "◄ ")
    const char* msg_marker = "- ";      ///< Marker for message events (e.g., "- ", "• ", "* ")
    
    // ANSI color support
    bool colorize_depth = false;        ///< Colorize output based on call depth (opt-in, ANSI colors)
    
    // Double-buffering for high-frequency tracing
    bool use_double_buffering = false;  ///< Enable double-buffering (opt-in, eliminates flush race conditions)
                                        ///< Pros: Safe concurrent write/flush, zero disruption, better for high-frequency tracing
                                        ///< Cons: 2x memory per thread (~4MB default), slightly more complex
                                        ///< Use when: Generating millions of events/sec with frequent flushing
    
    // Filtering and selective tracing
    struct {
        std::vector<std::string> include_functions;  ///< Include function patterns (empty = trace all)
        std::vector<std::string> exclude_functions;  ///< Exclude function patterns (higher priority than include)
        std::vector<std::string> include_files;      ///< Include file patterns (empty = trace all)
        std::vector<std::string> exclude_files;      ///< Exclude file patterns (higher priority than include)
        int max_depth = -1;                          ///< Maximum trace depth (-1 = unlimited)
    } filter;
    
    // Performance metrics and memory tracking
    bool print_stats = false;        ///< Print performance statistics at program exit
    bool track_memory = false;       ///< Sample RSS memory at each trace point (low overhead ~1-5µs)
    
    // Flush and shared memory behavior
    FlushMode flush_mode = FlushMode::OUTERMOST_ONLY;  ///< When to auto-flush on scope exit
    SharedMemoryMode shared_memory_mode = SharedMemoryMode::AUTO;  ///< Shared memory usage mode
    
    // Binary dump configuration
    const char* dump_prefix = "trace";  ///< Filename prefix for binary dumps (default: "trace")
    const char* dump_suffix = ".trc";   ///< File extension for binary dumps (default: ".trc")
    const char* output_dir = nullptr;   ///< Output directory (nullptr = current directory)
    
    /// Output directory layout options
    enum class OutputLayout {
        Flat,      ///< All files in output_dir: output_dir/trace_*.trc
        ByDate,    ///< Organized by date: output_dir/2025-10-20/trace_*.trc
        BySession  ///< Organized by session: output_dir/session_001/trace_*.trc
    };
    OutputLayout output_layout = OutputLayout::Flat;  ///< Directory structure layout (default: Flat)
    int current_session = 0;  ///< Session number for BySession layout (0 = auto-increment)
    
    /**
     * @brief Load configuration from INI file.
     * 
     * Parses an INI file and applies settings to this Config instance.
     * Supports sections: [output], [display], [formatting], [markers], [modes], 
     *                     [filter], [performance], [dump]
     * 
     * @param path Path to INI file (relative or absolute)
     * @return true on success, false if file not found or critical error
     * 
     * Example INI format:
     * @code
     * [display]
     * print_timing = true
     * print_timestamp = false
     * 
     * [dump]
     * prefix = trace
     * suffix = .trc
     * output_dir = logs
     * layout = date
     * 
     * [markers]
     * indent_marker = | 
     * enter_marker = -> 
     * @endcode
     */
    inline bool load_from_file(const char* path);
}

} // namespace trace


#endif // CONFIG_HPP
