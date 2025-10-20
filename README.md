# trace-scope

**Version:** 0.9.0-alpha

A lightweight, header-only C++ library for function-scope tracing with per-thread ring buffers.

## Features

- **TRACE_SCOPE()**: Automatic enter/exit recording with depth indentation and duration
- **TRACE_MSG(fmt, ...)**: Printf-style formatted message logging
- **TRACE_LOG**: Stream-based logging (C++ iostream style, drop-in for stream macros)
- **Per-thread ring buffers**: Lock-free writes, thread-safe flushing
- **Immediate mode**: Real-time output for long-running processes
- **Binary dump**: Compact `.trc` format with organized directory structures
- **Directory organization**: Flat, by-date, or by-session layouts
- **Batch processing**: Python analyzer processes directories of trace files
- **DLL-safe**: Optional shared state across DLL boundaries
- **Configurable output**: Timestamps, thread IDs, file/line info, function names
- **Header-only**: Just include and use (default mode)

## Quick Start

```cpp
#include <trace_scope.hpp>

void foo(int id, const std::string& name) {
    TRACE_SCOPE();
    TRACE_LOG << "Args: id=" << id << ", name=" << name;  // Stream-based
    TRACE_MSG("Processing data for id=%d", id);  // Printf-style
    // ... work ...
}

int main() {
    TRACE_SCOPE();
    foo(42, "test");
    trace::flush_all();
    return 0;
}
```

Output:
```
example.cpp:   8 foo                  -> foo
example.cpp:   9 foo                  - Processing data
example.cpp:   8 foo                  <- foo  [123.45 us]
example.cpp:  13 main                 -> main
example.cpp:  14 main                 <- main  [234.56 us]
```

## Development Tools

This project was developed using:
- **IDE:** Cursor (AI-powered code editor)
- **AI Model:** Claude Sonnet 4.5 (Anthropic)
- **Development Approach:** AI-assisted pair programming with iterative refinement
- **Testing:** Comprehensive test-driven development with 28+ C++ tests and 22+ Python tests

Key AI-assisted features implemented:
- Performance metrics system with zero-overhead design
- Statistical post-processing tools (call graphs, regression detection, trace diff)
- Runtime filtering with wildcard pattern matching
- Thread-aware color coding
- Binary format versioning with backward compatibility
- Cross-platform memory sampling (Windows/Linux/macOS)
- Multi-threaded ring buffer architecture with double-buffering option

## Configuration

All configuration is done via the global `trace::config` object:

```cpp
// Output destination
trace::config.out = std::fopen("trace.log", "w");

// Timing and metadata
trace::config.print_timing = true;          // Show function durations (default: true)
trace::config.print_timestamp = false;      // Show ISO timestamps (default: false, opt-in)
trace::config.print_thread = true;          // Show thread IDs (default: true)

// Prefix formatting
trace::config.include_file_line = true;     // Show file:line prefix (default: true)
trace::config.include_filename = true;      // Show filename (default: true)
trace::config.include_function_name = true; // Show function name (default: true)
trace::config.show_full_path = false;       // Show full vs basename (default: false)

// Column widths
trace::config.filename_width = 20;          // Fixed width for filename column
trace::config.line_width = 5;               // Fixed width for line numbers
trace::config.function_width = 20;          // Fixed width for function names

// Visual markers (customize appearance)
trace::config.show_indent_markers = true;   // Show indent markers (default: true)
trace::config.indent_marker = "| ";         // Indent marker (default: "| ")
trace::config.enter_marker = "-> ";         // Function entry marker (default: "-> ")
trace::config.exit_marker = "<- ";          // Function exit marker (default: "<- ")
trace::config.msg_marker = "- ";            // Message marker (default: "- ")
trace::config.colorize_depth = false;       // ANSI color by depth (default: false, opt-in)

// Tracing mode
trace::config.mode = trace::TracingMode::Buffered;  // Default: buffered
// Other modes: TracingMode::Immediate, TracingMode::Hybrid

// Advanced options
trace::config.auto_flush_at_exit = false;   // Auto-flush on scope exit (default: false, opt-in)
trace::config.use_double_buffering = false; // Enable double-buffering (default: false, opt-in)
```

#### Double-Buffering Mode

For extremely high-frequency tracing scenarios, double-buffering eliminates race conditions during flush operations.

**Compile-time requirement** (to save memory when not needed):

```bash
# Configure cmake with double-buffering enabled
cmake -DTRACE_DOUBLE_BUFFER=ON ..

# Or define before including header
#define TRACE_DOUBLE_BUFFER 1
#include <trace-scope/trace_scope.hpp>
```

**Then enable at runtime:**
```cpp
trace::config.use_double_buffering = true;  // Enable double-buffering
```

**Why compile-time?**
- ✅ **Saves ~1.2 MB per thread** when disabled (50% memory reduction)
- ✅ **Most users don't need it** - opt-in only for extreme scenarios
- ✅ **Smaller binaries** and faster compilation by default
- ⚠️ **Runtime error** if you set `use_double_buffering=true` without compiling it in

**Benefits:**
- **Eliminates race conditions** during flush operations
- **Zero disruption**: Write to one buffer while flushing the other
- **Safe concurrent operations**: No blocking between writes and flushes
- **Better performance** in high-frequency scenarios (millions of events/sec)

**Trade-offs:**
- **2x memory usage** per thread when enabled (~2.4MB with default settings)
- **Compile-time flag required** (prevents accidental memory waste)

**When to use:**
- Generating millions of trace events per second
- Frequent flush operations (e.g., every 10ms)
- Multiple threads with high-frequency tracing
- When you need guaranteed race-free flush operations

**Example:**
```cpp
// In CMakeLists.txt:
// set(TRACE_DOUBLE_BUFFER ON)

trace::config.use_double_buffering = true;

// Start high-frequency tracing...
// Flushes can happen concurrently without blocking writes
```

See `examples/example_double_buffer.cpp` for a complete demonstration.

### Customizing Visual Markers

You can customize the appearance of trace output with Unicode or ASCII markers:

```cpp
// Unicode style (fancy)
trace::config.indent_marker = "│ ";
trace::config.enter_marker = "↘ ";
trace::config.exit_marker = "↖ ";
trace::config.msg_marker = "• ";

// Box-drawing style
trace::config.indent_marker = "├─";
trace::config.enter_marker = "┌ ";
trace::config.exit_marker = "└ ";
trace::config.msg_marker = "│ ";

// Minimal ASCII
trace::config.indent_marker = "  ";
trace::config.enter_marker = "> ";
trace::config.exit_marker = "< ";
trace::config.msg_marker = ". ";

// No markers
trace::config.show_indent_markers = false;  // Use plain whitespace
```

### ANSI Color Support (Thread-Aware)

For easier visual tracking of call depths and threads, enable ANSI color-coding:

```cpp
trace::config.colorize_depth = true;  // Enable depth-based coloring
```

**Thread-Aware Color Coding:**

Each thread automatically gets a unique color offset based on its thread ID, making multi-threaded traces easy to follow:

- **Thread 1 (offset=0):** Red → Green → Yellow → Blue → Magenta → Cyan → White → Bright Red (cycles)
- **Thread 2 (offset=3):** Blue → Magenta → Cyan → White → Bright Red → Red → Green → Yellow (cycles)
- **Thread 3 (offset=5):** Cyan → White → Bright Red → Red → Green → Yellow → Blue → Magenta (cycles)

**Benefits:**
- Easy visual identification of which thread produced output
- Depth information still preserved (color changes with nesting)
- No configuration needed - automatic based on thread ID
- Makes debugging multi-threaded code significantly easier

**Example (Single-Threaded):**
```cpp
void foo() {
    TRACE_SCOPE();  // Depth 0 = Red
    bar();
}

void bar() {
    TRACE_SCOPE();  // Depth 1 = Green
    baz();
}

void baz() {
    TRACE_SCOPE();  // Depth 2 = Yellow
}
```

**Example (Multi-Threaded):**
```cpp
// Main thread
[0x1234] main()       // Red (offset=0, depth=0)
[0x1234]   process()  // Green (offset=0, depth=1)

// Worker thread (different color offset)
[0x5678] worker()     // Yellow (offset=3, depth=0) - Visually distinct!
[0x5678]   compute()  // Blue (offset=3, depth=1)
```

See `examples/example_thread_colors.cpp` for a complete demonstration.

**Notes:**
- Colors only work in ANSI-compatible terminals (most modern terminals)
- Windows Terminal, Linux terminals, macOS Terminal all support ANSI
- Old Windows cmd.exe may not display colors correctly (use Windows Terminal instead)
- Works with all marker styles (ASCII, Unicode, box-drawing)
- See `examples/example_colors.cpp` for a demonstration

### Configuration File Support

Instead of hardcoding configuration in your source code, you can load all settings from an INI file:

```cpp
#include <trace-scope/trace_scope.hpp>

int main() {
    // Load configuration from INI file
    trace::load_config("trace.conf");
    
    // Optional: Override specific settings programmatically
    trace::config.print_timestamp = true;
    
    // Use tracing as configured
    TRACE_SCOPE();
    // ...
}
```

**Example `trace.conf`:**
```ini
[output]
file = trace.log

[display]
print_timing = true
print_timestamp = false
print_thread = true
colorize_depth = false

[formatting]
filename_width = 20
line_width = 5
function_width = 20

[markers]
indent_marker = | 
enter_marker = -> 
exit_marker = <- 
message_marker = - 

[modes]
mode = buffered  # Options: buffered, immediate, hybrid
auto_flush_at_exit = false
use_double_buffering = false
auto_flush_threshold = 0.9
```

**Benefits:**
- ✅ **Separate config from code** - change settings without recompilation
- ✅ **Version control friendly** - commit configuration with your project
- ✅ **Team collaboration** - share trace settings easily
- ✅ **Environment-specific** - different configs for dev/test/prod
- ✅ **Zero dependencies** - built-in INI parser (~200 lines)

**INI File Format:**
- Sections: `[section_name]`
- Key-value pairs: `key = value`
- Comments: Lines starting with `#` or `;`
- Inline comments: `key = value  # comment`
- Quoted strings: `marker = "| "` (preserves spaces)
- Boolean values: `true`/`false`, `1`/`0`, `on`/`off`, `yes`/`no` (case-insensitive)

**For DLL projects:**
```cpp
int main() {
    // One line - DLL setup + config loading!
    TRACE_SETUP_DLL_SHARED_WITH_CONFIG("trace.conf");
    
    // Use tracing normally
    TRACE_SCOPE();
    // ...
}
```

See `examples/trace_config.ini` for a complete annotated example and `examples/example_config_file.cpp` for demonstration.

## Tracing Modes

trace-scope supports three tracing modes, controlled by a single enum:

```cpp
// Buffered mode (default) - best performance
trace::config.mode = trace::TracingMode::Buffered;

// Immediate mode - real-time output, no buffering
trace::config.mode = trace::TracingMode::Immediate;

// Hybrid mode - both buffered AND immediate
trace::config.mode = trace::TracingMode::Hybrid;
```

**Buffered Mode** (default):
- Events stored in per-thread ring buffers
- Manual flush required (`trace::flush_all()`)
- Best performance, lowest overhead (~10-50ns per trace)
- Events lost on crash if not flushed

**Immediate Mode** (async, v0.9.0+):
- Events printed to real-time output with minimal latency (~1-10ms)
- **Async I/O**: Background writer thread, non-blocking enqueue
- Low overhead (~5µs per trace, 10-20x better than old sync mode)
- Useful for real-time monitoring, debugging, production tracing
- Force synchronous flush: `trace::flush_immediate_queue()`
- Configurable flush interval (default: 1ms)

**Hybrid Mode** (async, v0.9.0+):
- Events both buffered AND printed immediately (async)
- Best of both worlds: real-time visibility + history
- Auto-flush when buffer reaches threshold
- Separate output streams possible (see Hybrid Mode section below)
- Same async benefits as Immediate mode

### Async Immediate Mode Details (v0.9.0+)

Immediate and Hybrid modes use async I/O for dramatically better performance:

**How it works:**
```cpp
trace::config.mode = trace::TracingMode::Immediate;
TRACE_SCOPE();  // Non-blocking: event queued in <1µs
// Background thread writes events every 1ms
```

**Configuration:**
```cpp
// Flush interval (default: 1ms)
trace::config.immediate_flush_interval_ms = 1;  // Lower = more real-time, higher = better throughput

// Queue size (default: 128)
trace::config.immediate_queue_size = 128;  // Larger = better batching
```

**Force Synchronous Flush:**
```cpp
// When you need guarantees (before crash, in tests)
trace::flush_immediate_queue();  // Blocks until all events written

// Example: Critical section
{
    TRACE_SCOPE();
    critical_operation();
    trace::flush_immediate_queue();  // Ensure events written before proceeding
}
```

**Performance:**
- Buffered mode: ~10-50ns overhead (baseline)
- Async immediate: ~5µs overhead (100x better than old sync mode)
- Real-time latency: 1-10ms (events appear nearly instantly)

**When to use:**
- Real-time monitoring of production systems
- Debugging with live output
- Long-running processes where buffered mode might lose data
- Multi-threaded applications (better scaling than old sync mode)

See `examples/example_async_immediate.cpp` for demonstrations and `examples/example_async_benchmark.cpp` for performance comparison.

## Filtering and Selective Tracing

Focus tracing on specific functions, files, or depth ranges using runtime filters with simple wildcard patterns.

### Programmatic Filtering

```cpp
#include <trace-scope/trace_scope.hpp>

int main() {
    // Include only specific functions
    trace::filter_include_function("my_namespace::*");
    trace::filter_include_function("core_*");
    
    // Exclude test/debug functions
    trace::filter_exclude_function("*_test");
    trace::filter_exclude_function("debug_*");
    
    // Limit call depth
    trace::filter_set_max_depth(15);
    
    // Use tracing - only matching functions/files traced
    TRACE_SCOPE();
}
```

### Configuration File Filtering

```ini
[filter]
include_function = my_namespace::*
include_function = core_*
exclude_function = *_test
exclude_function = debug_*

include_file = src/core/*
exclude_file = */test/*

max_depth = 15
```

### How Filters Work

**Function Filters:**
- **Include**: Only trace functions matching these patterns (empty = trace all)
- **Exclude**: Never trace these functions (overrides include)
- **Patterns**: Use `*` wildcard (e.g., `test_*`, `*_debug`, `*mid*`)

**File Filters:**
- **Include**: Only trace files matching these patterns (empty = trace all)
- **Exclude**: Never trace these files (overrides include)
- **Patterns**: Use `*` for paths (e.g., `src/core/*`, `*/test/*`)

**Depth Filter:**
- `max_depth = N` - don't trace beyond depth N
- -1 = unlimited (default)
- Helps avoid deep recursion spam

**Exclude always wins:** If a function/file matches both include and exclude, it's excluded.

### Filter API

```cpp
// Function filters
trace::filter_include_function("core::*");
trace::filter_exclude_function("*_test");

// File filters
trace::filter_include_file("src/networking/*");
trace::filter_exclude_file("*/tests/*");

// Depth filter
trace::filter_set_max_depth(10);

// Clear all filters (restore to trace everything)
trace::filter_clear();
```

### Use Cases

**Focus on specific module:**
```cpp
trace::filter_include_file("src/networking/*");
```

**Exclude test code:**
```cpp
trace::filter_exclude_function("*_test");
trace::filter_exclude_file("*/tests/*");
```

**Limit call depth:**
```cpp
trace::filter_set_max_depth(10);  // Prevent deep recursion spam
```

**Complex filtering:**
```ini
[filter]
# Only trace core module
include_file = src/core/*

# But exclude tests and debug functions
exclude_function = *_test
exclude_function = debug_*
exclude_file = */test/*

# Limit depth
max_depth = 12
```

### Wildcard Patterns

- `*` matches zero or more characters
- Case-sensitive matching
- Examples:
  - `test_*` matches `test_foo`, `test_bar_baz`
  - `*_test` matches `my_test`, `foo_bar_test`
  - `*mid*` matches `midpoint`, `pyramid`, `mid`
  - `*::*` matches `namespace::function`

### Performance Notes

- Filters checked once per TRACE_SCOPE() call
- Filtered events never written to ring buffer (saves memory)
- Minimal overhead (string comparison only when tracing)
- Typical use: < 10 patterns, negligible impact

See `examples/example_filtering.cpp` for comprehensive demonstration.

## Performance Metrics

trace-scope provides built-in performance metrics collection with **zero tracing overhead**. Metrics are computed by scanning existing ring buffers at program exit or via the Python post-processing tool.

### Key Features

- **Zero Overhead**: No extra work during tracing (unless memory tracking enabled)
- **Automatic**: Optional statistics at program exit (C++ runtime)
- **Comprehensive**: Detailed analysis via Python tool
- **Filtered**: Apply filters before computing stats
- **Export**: CSV and JSON export for external analysis

### C++ Runtime Stats (Basic)

Enable automatic performance statistics at program exit:

```cpp
#include <trace-scope/trace_scope.hpp>

int main() {
    trace::config.print_stats = true;      // Enable automatic stats at exit
    trace::config.track_memory = true;     // Optional: sample RSS memory (low overhead ~1-5µs)
    
    TRACE_SCOPE();
    
    // Your code here...
    
    // Binary dump for detailed Python analysis
    trace::dump_binary("trace.trc");
    
    // Stats automatically printed at exit
    return 0;
}
```

**Output:**
```
================================================================================
 Performance Metrics Summary
================================================================================

Global Statistics:
--------------------------------------------------------------------------------
Function                                      Calls        Total          Avg          Min          Max       Memory
--------------------------------------------------------------------------------
slow_function                                     6     49.26 ms      8.21 ms      3.30 ms     11.26 ms     15.12 MB
memory_intensive_function                         3     54.57 ms     18.19 ms     13.25 ms     22.11 ms     13.27 MB
fast_function                                    11     93.60 us      8.51 us      6.80 us     15.10 us     15.12 MB

Per-Thread Breakdown:
================================================================================

Thread 0x052b0a09 (20 events, peak RSS: 15.12 MB):
--------------------------------------------------------------------------------
Function                                      Calls        Total          Avg       Memory
--------------------------------------------------------------------------------
slow_function                                     6     49.26 ms      8.21 ms     15.12 MB
memory_intensive_function                         3     54.57 ms     18.19 ms     13.27 MB
fast_function                                    11     93.60 us      8.51 us     15.12 MB
================================================================================
```

**What it shows:**
- **Calls**: Number of times each function was called
- **Total**: Total execution time across all calls
- **Avg**: Average execution time per call
- **Min/Max**: Fastest and slowest execution times
- **Memory**: Peak RSS memory usage (when `track_memory` enabled)
- **Per-Thread**: Breakdown by thread (when multiple threads exist)

### Python Tool Stats (Comprehensive)

The Python post-processing tool provides more detailed analysis:

```bash
# Display performance statistics
python tools/trc_analyze.py stats trace.trc

# Sort by different metrics
python tools/trc_analyze.py stats trace.trc --sort-by calls     # by call count
python tools/trc_analyze.py stats trace.trc --sort-by avg       # by average time
python tools/trc_analyze.py stats trace.trc --sort-by name      # alphabetically

# Filter before computing stats
python tools/trc_analyze.py stats trace.trc --filter-function "worker*"

# Export to CSV or JSON
python tools/trc_analyze.py stats trace.trc --export-csv stats.csv
python tools/trc_analyze.py stats trace.trc --export-json stats.json
```

### Memory Tracking

Memory tracking is **optional** and disabled by default for zero overhead:

```cpp
trace::config.track_memory = true;  // Sample RSS at each TRACE_SCOPE (low overhead ~1-5µs)
```

**What it measures:**
- **RSS (Resident Set Size)**: Total process memory usage
- **Per-Function Delta**: Peak RSS during function execution
- **Not Precise**: Includes all allocations, not just the function's own
- **Use Case**: Find memory-hungry code paths

**Performance Impact:**
- **Disabled (default)**: Zero overhead
- **Enabled**: ~1-5 microseconds per TRACE_SCOPE call
- System calls: `GetProcessMemoryInfo` (Windows), `/proc/self/status` (Linux), `task_info` (macOS)

### Configuration File

Add to your INI file:

```ini
[performance]
print_stats = true       # Print stats at program exit
track_memory = true      # Sample RSS memory at each trace point
```

### Use Cases

1. **Identify Hotspots**: Find slowest functions
2. **Optimize Allocations**: Track memory-hungry code paths
3. **Multi-threaded Analysis**: Compare performance across threads
4. **Regression Testing**: Export stats to CSV/JSON for CI/CD
5. **Production Profiling**: Low-overhead performance monitoring

### Example

See `examples/example_stats.cpp` for a complete demonstration with multi-threaded workloads and memory tracking.

## Stream-Based Logging (TRACE_LOG)

For C++ iostream-style logging, use `TRACE_LOG`:

```cpp
void process(int id, const std::string& name) {
    TRACE_SCOPE();
    TRACE_LOG << "Args: id=" << id << ", name=" << name;
    
    TRACE_LOG << "Processing item " << id;
    // work...
}
```

### Printf vs Stream Style

Both styles work equally well - choose based on preference:

```cpp
// Printf-style (traditional C)
TRACE_MSG("Processing id=%d, name=%s", id, name.c_str());

// Stream-style (C++ iostream)
TRACE_LOG << "Processing id=" << id << ", name=" << name;
```

**When to use which:**
- **TRACE_MSG**: When you have printf format strings, simple values
- **TRACE_LOG**: When logging complex types, custom operator<<, drop-in for existing stream code

## DLL Boundary Support (Header-Only Solution)

### The Problem

By default, trace_scope is header-only. When used across multiple DLLs, each DLL gets its own copy of the global state (config and registry). This means traces from different DLLs won't be combined.

### Simple Solution: TRACE_SETUP_DLL_SHARED() Macro

**Just one line in your main executable:**

```cpp
#include <trace-scope/trace_scope.hpp>

int main() {
    TRACE_SETUP_DLL_SHARED();  // That's it! Automatic setup & cleanup
    
    // Configure as needed (use get_config() to access shared config)
    trace::get_config().out = std::fopen("trace.log", "w");
    
    // Use tracing normally - all DLLs share the same state!
    TRACE_SCOPE();
    call_dll_functions();
    
    return 0;  // Automatic flush via RAII
}
```

**With configuration file:**

```cpp
int main() {
    // Even simpler - DLL setup + config loading in one line!
    TRACE_SETUP_DLL_SHARED_WITH_CONFIG("trace.conf");
    
    // Use tracing normally - configured from file
    TRACE_SCOPE();
    call_dll_functions();
    
    return 0;  // Automatic flush via RAII
}
```

**That's it!** Completely header-only, automatic cleanup, works across any number of DLLs.

**Benefits:**
- ✅ **1 line of code** (was 20+ lines before)
- ✅ **Automatic cleanup** via RAII (flush on exit)
- ✅ **Exception-safe** (cleanup even if exceptions occur)
- ✅ **No manual flush needed** (destructor handles it)
- ✅ **Zero overhead** when not used

See `examples/example_dll_shared.cpp` for complete demonstration.

### Advanced: Manual Setup

For advanced users who need more control, you can still use `set_external_state()` manually:

```cpp
namespace {
    trace::Config g_trace_config;
    trace::Registry g_trace_registry;
}

int main() {
    trace::set_external_state(&g_trace_config, &g_trace_registry);
    // ... manual cleanup with trace::flush_all()
}
```

### How It Works

- `TRACE_SETUP_DLL_SHARED()` creates shared state instances with RAII guard
- `set_external_state()` sets global pointers to your instances
- All trace operations check these pointers first
- Falls back to default instances if not set
- Thread-safe and zero overhead when not used

### Alternative: Compilation-Based Sharing (Advanced)

If you have control over the build system and prefer compile-time linking:

1. Include `src/trace_scope_impl.cpp` in ONE compilation unit
2. Define `TRACE_SCOPE_SHARED` when compiling all files
3. See `src/trace_scope_impl.cpp` for detailed instructions

This approach requires modifying your build system but avoids the runtime setup call.

## Binary Dump Format

For compact storage and post-processing, the library provides automatic timestamped binary dumps with configurable directory organization:

### Simple Usage (Current Directory, Flat Layout)

```cpp
// Automatic timestamped filename (default: current directory, flat layout)
std::string filename = trace::dump_binary();
// Returns: "trace_20251020_162817_821.trc"

// Custom prefix
std::string filename = trace::dump_binary("myapp");
// Returns: "myapp_20251020_103045_123.trc"
```

### Organized Output with Directory Structures

```cpp
// Flat layout - all files in one directory
trace::config.output_dir = "logs";
trace::config.output_layout = trace::Config::OutputLayout::Flat;
std::string filename = trace::dump_binary();
// Returns: "logs/trace_20251020_162817_821.trc"

// ByDate layout - organized by date
trace::config.output_layout = trace::Config::OutputLayout::ByDate;
std::string filename = trace::dump_binary();
// Returns: "logs/2025-10-20/trace_162817_821.trc"

// BySession layout - organized by session with auto-increment
trace::config.output_layout = trace::Config::OutputLayout::BySession;
trace::config.current_session = 0;  // 0 = auto-increment
std::string filename = trace::dump_binary();
// Returns: "logs/session_001/trace_20251020_162817_821.trc"

// Manual session number
trace::config.current_session = 5;
std::string filename = trace::dump_binary();
// Returns: "logs/session_005/trace_20251020_162817_821.trc"
```

### Configuration via INI File

```ini
[dump]
prefix = myapp
suffix = .trc
output_dir = logs
layout = date      # Options: flat, date, session
session = 0        # 0 = auto-increment
```

**Benefits:**
- ✅ **No data loss**: Each call creates a new unique file
- ✅ **Automatic directory creation**: Creates output directories if needed
- ✅ **Organized by date**: Easy to find traces from specific days
- ✅ **Session-based organization**: Group related trace runs together
- ✅ **Auto-increment sessions**: Automatically finds highest session and increments
- ✅ **Chronological ordering**: Files sort naturally by timestamp
- ✅ **Millisecond precision**: Unique even with rapid dumps
- ✅ **Descriptive extension**: `.trc` clearly indicates trace files

**Filename Format:** `{prefix}_{YYYYMMDD}_{HHMMSS}_{milliseconds}{suffix}`

See `examples/example_long_running.cpp` for periodic dump demonstration and `examples/example_test_v08.cpp` for directory layout examples.

### Python Post-Processing Tool

The `tools/trc_analyze.py` tool provides powerful post-processing of binary trace dumps with multi-command interface for analysis, filtering, visualization, and batch processing.

**Features:**
- ✅ Thread-aware ANSI color output (matches runtime colors)
- ✅ Wildcard filtering (function, file, depth, thread)
- ✅ Include/exclude filter lists (exclude wins)
- ✅ Directory batch processing (process multiple trace files)
- ✅ Recursive subdirectory search
- ✅ Chronological/name/size file sorting
- ✅ Binary format versions 1 and 2 support
- ✅ Auto-scaling duration units (ns/us/ms/s)
- ✅ Visual indent markers for call depth
- ✅ Version information (--version flag)

**Basic Usage:**
```bash
# Check version
python tools/trc_analyze.py --version

# Display single trace file
python tools/trc_analyze.py display trace.trc

# Display all traces in a directory
python tools/trc_analyze.py display logs/

# Display all traces recursively (includes subdirectories)
python tools/trc_analyze.py display logs/ --recursive

# With thread-aware colors
python tools/trc_analyze.py display trace.trc --color

# Filter to specific functions
python tools/trc_analyze.py display trace.trc --filter-function "core_*"

# Exclude test functions
python tools/trc_analyze.py display trace.trc --exclude-function "*_test" --exclude-function "debug_*"

# Limit depth
python tools/trc_analyze.py display trace.trc --max-depth 10

# Filter by thread ID
python tools/trc_analyze.py display trace.trc --filter-thread 0x1234

# Complex filtering with colors
python tools/trc_analyze.py display trace.trc --color --max-depth 8 \
    --filter-function "my_namespace::*" \
    --exclude-function "debug_*" \
    --exclude-file "*/test/*"

# Show performance statistics (single file)
python tools/trc_analyze.py stats trace.trc

# Show aggregated statistics from directory
python tools/trc_analyze.py stats logs/ --recursive

# Sort files by name before processing
python tools/trc_analyze.py stats logs/ --sort-files name
```

**Command-Line Options:**

| Option | Description |
|--------|-------------|
| `--version` | Show version information |
| `--color` | Enable thread-aware ANSI colors |
| `--recursive` / `-r` | Recursively search subdirectories for trace files |
| `--sort-files SORT` | Sort order for multiple files: chronological (default), name, size |
| `--filter-function PATTERN` | Include functions matching pattern (wildcard `*`) |
| `--exclude-function PATTERN` | Exclude functions matching pattern |
| `--filter-file PATTERN` | Include files matching pattern |
| `--exclude-file PATTERN` | Exclude files matching pattern |
| `--filter-thread TID` | Include specific thread IDs (hex: 0x1234 or decimal) |
| `--exclude-thread TID` | Exclude specific thread IDs |
| `--max-depth N` | Maximum call depth to display (-1 = unlimited) |
| `--no-timing` | Hide duration timing |
| `--timestamp` | Show absolute timestamps |

**Output Format:**
```
(thread_id) | | -> function_name (file:line)
(thread_id) | | | - message text (file:line)
(thread_id) | | <- function_name  [duration] (file:line)
```

**Benefits:**
- Post-process traces with different filters without re-running program
- Apply filters to existing binary dumps
- Analyze specific subsystems or threads
- Same filtering logic as runtime (consistent behavior)
- Thread-aware colors match runtime output exactly

**Binary Format Validation:**

The `test_binary_format` test ensures the Python parser stays synchronized with the C++ binary format:

```bash
# From build directory:
./test_binary_format

# This test:
# 1. Generates trace events
# 2. Dumps to binary file
# 3. Runs Python parser automatically
# 4. Verifies successful parsing
```

If the binary format changes in the C++ code, this test will fail, alerting you to update the Python parser.

**Binary format specification (TRCLOG10 v1):**
```
Header:    "TRCLOG10" (8 bytes) + version(4) + padding(4)
Per event: type(1) + tid(4) + ts_ns(8) + depth(4) + dur_ns(8) +
           file_len(2) + file_str + func_len(2) + func_str +
           msg_len(2) + msg_str + line(4)
```

### Automatic Instrumentation Tool

The `tools/trace_instrument.py` utility automatically adds or removes `TRACE_SCOPE()` macros from C++ files:

**Add TRACE_SCOPE() to all functions:**
```bash
python tools/trace_instrument.py add myfile.cpp
```

**Remove all TRACE_SCOPE() calls:**
```bash
python tools/trace_instrument.py remove myfile.cpp
```

**Process multiple files:**
```bash
python tools/trace_instrument.py add src/*.cpp
```

**Features:**
- Automatically detects function definitions (free functions, methods, constructors)
- Preserves correct indentation
- Creates `.bak` backup files before modifying
- Skips functions that already have TRACE_SCOPE()
- Handles namespaces, classes, and templates (most cases)

**Example:**
```cpp
// Before:
void my_function(int x) {
    std::cout << "Processing " << x << std::endl;
}

// After running: python tools/trace_instrument.py add file.cpp
void my_function(int x) {
    TRACE_SCOPE();
    std::cout << "Processing " << x << std::endl;
}
```

**Notes:**
- Uses regex-based parsing (simple but effective)
- May need manual adjustment for very complex template or macro code
- Always creates backups for safety
- Review changes before committing instrumented code

## Statistical Post-Processing

The `trc_analyze.py` tool provides advanced post-processing features for analyzing trace data, detecting regressions, and visualizing call relationships.

### Commands Overview

```bash
trc_analyze.py <command> [OPTIONS]

Commands:
  display    - Pretty-print trace with filtering and colors
  stats      - Performance metrics and statistics
  callgraph  - Call graph generation (text tree, GraphViz DOT)
  compare    - Performance regression detection
  diff       - Execution path comparison
  query      - Enhanced filtering/querying (coming soon)
```

### Call Graph Generation

Visualize function call relationships, call counts, and timing information.

**Text Tree Output:**
```bash
# Generate call graph as text tree
python tools/trc_analyze.py callgraph trace.trc

# Output:
# └── main calls=1 total=10.00 ms avg=10.00 ms
#     ├── foo calls=3 total=6.50 ms avg=2.17 ms
#     │   └── helper calls=3 total=1.20 ms avg=400.00 us
#     └── bar calls=2 total=3.40 ms avg=1.70 ms

# Save to file
python tools/trc_analyze.py callgraph trace.trc --output callgraph.txt
```

**GraphViz DOT Format:**
```bash
# Generate GraphViz DOT file
python tools/trc_analyze.py callgraph trace.trc --format dot --output callgraph.dot

# Generate PNG image (requires GraphViz installed)
dot -Tpng callgraph.dot -o callgraph.png

# Features:
# - Nodes colored by total duration (heatmap)
# - Edge labels show call counts
# - Edge thickness indicates frequency
```

**Options:**
- `--format tree|dot` - Output format (default: tree)
- `--output FILE` - Save to file instead of stdout
- `--no-counts` - Hide call counts
- `--no-durations` - Hide timing information
- `--tree-max-depth N` - Limit tree depth
- `--no-color` - Disable DOT node coloring
- `--filter-function PATTERN` - Apply filters (same as display/stats)

**Use Cases:**
1. Understand code structure and call relationships
2. Identify most frequently called functions
3. Find critical paths (longest execution chains)
4. Detect recursion patterns
5. Generate documentation visualizations

### Performance Regression Detection

Compare two trace files to detect performance changes.

**Basic Comparison:**
```bash
# Compare baseline vs current
python tools/trc_analyze.py compare baseline.trc current.trc

# Output shows:
# - Regressions (slower functions, higher memory)
# - Improvements (faster functions, lower memory)
# - New functions (only in current)
# - Removed functions (only in baseline)
```

**Advanced Options:**
```bash
# Set threshold (only report >=10% changes)
python tools/trc_analyze.py compare baseline.trc current.trc --threshold 10

# Show all results (not just top N)
python tools/trc_analyze.py compare baseline.trc current.trc --show-all

# Export to CSV/JSON
python tools/trc_analyze.py compare baseline.trc current.trc --export-csv regression.csv

# Fail if regressions found (for CI/CD)
python tools/trc_analyze.py compare baseline.trc current.trc --fail-on-regression
```

**Example Output:**
```
====================================================================================================
 Performance Comparison Report
====================================================================================================

⚠ REGRESSIONS DETECTED (2):
----------------------------------------------------------------------------------------------------
Function                                 Metric                 Baseline         Current          Change
----------------------------------------------------------------------------------------------------
slow_function                            avg_ns                 16.22 ms        28.91 ms          +78.2%
memory_function                          memory_delta            4.00 MB         8.00 MB         +100.0%

✓ IMPROVEMENTS (1):
----------------------------------------------------------------------------------------------------
improved_function                        avg_ns                 10.00 ms         5.00 ms          -50.0%

+ NEW FUNCTIONS (1):
  + new_function

- REMOVED FUNCTIONS (1):
  - removed_function
```

**Use Cases:**
1. CI/CD regression testing (--fail-on-regression)
2. Performance optimization validation
3. Pre-merge performance checks
4. Release candidate validation
5. A/B testing different implementations

**Example Integration:**
See `examples/example_regression.cpp` for generating baseline and current traces with known performance differences.

### Execution Path Diff

Compare execution paths between two trace runs to detect behavioral differences.

**Basic Diff:**
```bash
# Compare execution paths
python tools/trc_analyze.py diff trace_a.trc trace_b.trc

# Export to JSON
python tools/trc_analyze.py diff old.trc new.trc --export-json diff_report.json
```

**What It Detects:**
- Functions called in A but not in B
- Functions called in B but not in A
- Different call orders (sequence differences)
- Different call depths

**Use Cases:**
1. Validate refactoring didn't change behavior
2. Debug test failures (compare passing vs failing runs)
3. Detect side effects from code changes
4. Verify feature flags work correctly
5. Compare different execution modes

### Filtering Support

All post-processing commands support filtering:

```bash
# Callgraph for core functions only
python tools/trc_analyze.py callgraph trace.trc --filter-function "core_*"

# Compare specific subsystem
python tools/trc_analyze.py compare baseline.trc current.trc \
    --filter-file "src/networking/*"

# Diff specific thread
python tools/trc_analyze.py diff trace_a.trc trace_b.trc \
    --filter-thread 0x1234
```

**Filter Options:**
- `--filter-function PATTERN` - Include functions matching wildcard pattern
- `--exclude-function PATTERN` - Exclude functions
- `--filter-file PATTERN` - Include files matching pattern
- `--exclude-file PATTERN` - Exclude files
- `--filter-thread TID` - Include specific thread IDs
- `--exclude-thread TID` - Exclude thread IDs
- `--max-depth N` - Maximum call depth

## Build-Time Configuration

Control buffer sizes and features at compile time:

```cpp
#define TRACE_ENABLED 1        // Enable/disable all tracing (default: 1)
#define TRACE_RING_CAP 4096    // Events per thread (default: 4096)
#define TRACE_MSG_CAP 192      // Max message size (default: 192)
#define TRACE_DEPTH_MAX 512    // Max call depth tracked (default: 512)
#define TRACE_DOUBLE_BUFFER 0  // Enable double-buffering (default: 0, saves ~1.2MB/thread)

#include <trace-scope/trace_scope.hpp>
```

**Or in CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.16)
project(my_project)

# Enable double-buffering if needed
set(TRACE_DOUBLE_BUFFER ON)  # OFF by default to save memory

add_subdirectory(trace-scope)
target_link_libraries(my_app PRIVATE trace_scope)
```

## Thread Safety

- **Writes**: Lock-free per-thread ring buffers
- **Flush**: Thread-safe with mutex protection
- **Immediate mode**: Thread-safe with mutex-serialized output
- **Config changes**: Not thread-safe (set before spawning threads)

## Auto-Scaling Duration Units

Durations automatically scale for readability:
- `< 1 us`: nanoseconds (e.g., `[123 ns]`)
- `< 1 ms`: microseconds (e.g., `[45.67 us]`)
- `< 1 s`: milliseconds (e.g., `[123.45 ms]`)
- `≥ 1 s`: seconds (e.g., `[2.345 s]`)

## Examples

See `examples/` directory:
- `example_basic.cpp`: Basic usage with multi-threaded tracing and stream logging
- `example_dll_shared.cpp`: Header-only DLL state sharing demonstration
- `example_custom_markers.cpp`: Customizing visual markers (ASCII, Unicode, box-drawing)
- `example_colors.cpp`: ANSI color-coded output by call depth
- `src/trace_scope_impl.cpp`: Advanced DLL compilation-based sharing template

See `tests/` directory:
- `test_trace.cpp`: Basic functionality tests
- `test_comprehensive.cpp`: Extensive test suite covering all features
- `test_binary_format.cpp`: Binary format and Python parser validation

## Tools

See `tools/` directory:

**`trc_analyze.py`** - Trace analysis tool with multiple commands
- Multi-command interface for display, stats, callgraph, compare, diff, query
- Usage: `python tools/trc_analyze.py <command> trace.trc`
- Commands: `display`, `stats`, `callgraph`, `compare`, `diff`, `query`
- Supports directory processing with `--recursive` flag
- See "Binary Dump Format" section for details

**`trace_instrument.py`** - Automatic code instrumentation
- Adds/removes TRACE_SCOPE() from C++ files
- Usage: `python tools/trace_instrument.py add file.cpp`
- Usage: `python tools/trace_instrument.py remove file.cpp`
- Tests: `python tools/test_trace_instrument.py`
- See "Automatic Instrumentation Tool" section for details

**Python Tool Tests:**
- `test_trace_instrument.py` - Unit tests for instrumentation tool (11 tests)
- `test_trc_analyze.py` - Unit tests for binary parser and filtering (10 tests)
- `test_trc_callgraph.py` - Unit tests for call graph generation (5 tests)
- `test_trc_compare.py` - Unit tests for regression detection (3 tests)
- `test_trc_diff.py` - Unit tests for trace diff (4 tests)
- All require Python 3.6+ (standard library only - no pip install needed)
- See `tools/requirements.txt` for details
- Run tests to verify tools work correctly on your system

## Building

```bash
mkdir build && cd build
cmake ..
cmake --build .

# Run tests
./test_trace
./test_comprehensive

# Run example
./example_basic
```

## Requirements

**C++ Library:**
- C++17 or later
- CMake 3.16+ (for building tests/examples)
- Windows: MSVC 2019+, MinGW, or Clang
- Linux/Mac: GCC 7+, Clang 5+

**Python Tools** (optional):
- Python 3.6+ (for trc_analyze.py, trace_instrument.py)
- **Zero external dependencies** - uses only Python standard library
- See `tools/requirements.txt` for details and optional enhancements
- Optional: GraphViz (for rendering call graph DOT files to PNG/SVG)

## License

See LICENSE file.

## Performance Considerations

**Buffered mode (default)**:
- Minimal overhead (~10-50ns per trace call)
- Lock-free writes to per-thread buffer
- Ring buffer wraps on overflow (oldest events lost)
- Flush when convenient

**Immediate mode (async, v0.9.0+)**:
- Low overhead (~5µs per trace call)
- Non-blocking enqueue to async queue
- Background writer thread with batched I/O
- Real-time visibility (1-10ms latency)
- No data loss on crash (with `flush_immediate_queue()` at checkpoints)
- 10-20x better than old synchronous immediate mode

**Hybrid mode (async, v0.9.0+)**:
- Combines buffered + async immediate
- Overhead similar to immediate mode (~5µs)
- Dual outputs: buffered history + real-time monitoring
- Background auto-flush when buffer reaches threshold

**Performance Comparison:**

| Mode | Overhead/Trace | Best For |
|------|----------------|----------|
| Buffered | ~10-50ns | Performance-critical code, hot paths |
| Async Immediate | ~5µs | Real-time monitoring, debugging, production |
| Hybrid | ~5µs | Development, testing, comprehensive logging |

**Recommendations**:
- Use **buffered mode** for performance-critical code (100x faster)
- Use **immediate mode** for real-time monitoring and debugging (still low overhead)
- Use **hybrid mode** when you need both history and real-time output
- Call `flush_immediate_queue()` before critical operations if using immediate/hybrid
- Disable tracing in release builds (`#define TRACE_ENABLED 0`) if not needed

## Troubleshooting

**No output**: Call `trace::flush_all()` or enable `auto_flush_at_exit`

**Missing main exit**: Enable `trace::config.auto_flush_at_exit = true`

**Truncated output**: Increase `TRACE_RING_CAP` if ring buffer wraps

**DLL not sharing state**: Ensure TRACE_SCOPE_SHARED defined for all compilation units

**Long delays on exit**: Disable `auto_flush_at_exit` and manually flush before returning

## Development & Testing

### Building from Source

```bash
# Clone the repository
git clone https://github.com/eytree/trace-scope.git
cd trace-scope

# Create build directory
mkdir build && cd build

# Configure with CMake (Ninja or your preferred generator)
cmake -G Ninja ..

# Build all targets (examples + tests)
cmake --build .

# Or build specific targets
cmake --build . --target test_comprehensive
cmake --build . --target example_basic
```

### Running Tests

trace-scope includes a lightweight, dependency-free test framework that supports running individual tests or test suites.

**Run all tests:**
```bash
./test_comprehensive              # Run all 12 tests
./test_double_buffer               # Run all double-buffer tests
./test_framework_test              # Run framework self-tests
```

**List available tests:**
```bash
./test_comprehensive --list
# Output:
# Registered tests (12 total):
#   [1] multi_threaded_trace
#   [2] immediate_vs_buffered
#   [3] config_combinations
#   ...
```

**Run specific tests:**
```bash
# Run tests matching "buffer"
./test_comprehensive buffer
# Runs: immediate_vs_buffered, ring_buffer_wraparound

# Run tests matching "timing"
./test_comprehensive timing
# Runs: timing_accuracy

# Run specific test by exact name match
./test_double_buffer functional
# Runs: functional test only
```

**Command-line options:**
- No arguments: Run all tests
- `test_name`: Run tests containing this substring
- `--list` or `-l`: List all registered tests
- `--help` or `-h`: Show usage information

### Test Framework

The test framework (`tests/test_framework.hpp`) is a header-only, dependency-free testing system designed specifically for trace-scope:

**Features:**
- Automatic test registration via `TEST()` macro
- Rich assertion macros: `TEST_ASSERT()`, `TEST_ASSERT_EQ()`, `TEST_ASSERT_NE()`
- Selective test execution via command-line filtering
- Clear pass/fail reporting with detailed error messages
- Exception-safe test isolation
- Zero external dependencies

**Writing new tests:**
```cpp
#include "test_framework.hpp"
#include <trace-scope/trace_scope.hpp>

TEST(my_feature_test) {
    // Setup
    trace::config.out = std::fopen("my_test.log", "w");
    
    // Test your feature
    TRACE_SCOPE();
    TRACE_MSG("Testing feature");
    
    // Assertions
    TEST_ASSERT(some_condition, "Feature should work");
    TEST_ASSERT_EQ(actual_value, expected_value, "Values match");
    
    // Cleanup
    trace::flush_all();
    std::fclose(trace::config.out);
    trace::config.out = stdout;
}

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
```

**Current test coverage:**
- `test_framework_test`: 10 tests validating the framework itself
- `test_comprehensive`: 12 tests covering core functionality
- `test_double_buffer`: 5 tests for double-buffering mode
- `test_trace`: Multi-threaded tracing and binary dumps
- `test_binary_format`: Binary format compatibility with Python parser

All tests run cleanly with **zero external dependencies** - just C++17 standard library.

## Roadmap

### ✅ Recently Completed

**Hybrid Buffered + Immediate Mode** *(Implemented)*
- ✅ Simultaneous buffered and immediate output
- ✅ Auto-flush ring buffer when near capacity (configurable threshold)
- ✅ Separate output streams for immediate vs buffered
- ✅ Prevents data loss while maintaining performance
- See `example_hybrid.cpp` for demonstration

**Double-Buffering Mode** *(Implemented)*
- ✅ Optional double-buffering for high-frequency tracing
- ✅ Eliminates race conditions during flush operations
- ✅ Safe concurrent write/flush operations
- ✅ Runtime configurable via `trace::config.use_double_buffering`
- See `example_double_buffer.cpp` and double-buffering section above

**Comprehensive Test Framework** *(Implemented)*
- ✅ Lightweight, zero-dependency test framework (`test_framework.hpp`)
- ✅ Selective test execution via command-line filtering
- ✅ 28 tests covering all functionality
- ✅ Rich assertion macros and clear reporting
- See Development & Testing section above

**Filtering & Selective Tracing** *(Implemented)*
- ✅ Wildcard pattern matching for functions and files
- ✅ Include/exclude filter lists (exclude wins priority)
- ✅ Max depth limiting to prevent recursion spam
- ✅ INI configuration support with `[filter]` section
- ✅ 22 comprehensive tests, all passing
- ✅ Runtime configurable - no recompilation needed
- See Filtering and Selective Tracing section above

**Example:**
```cpp
trace::filter_include_function("core_*");      // Only trace core functions
trace::filter_exclude_function("*_test");      // Skip test functions
trace::filter_include_file("src/networking/*"); // Only trace networking
trace::filter_set_max_depth(10);               // Limit depth
```

**Performance Metrics & Analysis** *(Implemented)*
- ✅ Zero overhead during tracing (computed at exit/flush)
- ✅ Per-function call count, min/max/avg duration statistics
- ✅ Memory tracking (optional RSS sampling, ~1-5µs overhead)
- ✅ Global and per-thread breakdown
- ✅ Python tool with filtering, sorting, CSV/JSON export
- ✅ Automatic stats at program exit (C++) or on-demand (Python)
- See Performance Metrics section above

**Example:**
```cpp
trace::config.print_stats = true;       // Automatic stats at exit
trace::config.track_memory = true;      // Optional RSS tracking
trace::internal::ensure_stats_registered();  // Register handler

// Python tool
python tools/trc_analyze.py stats trace.trc --sort-by total
python tools/trc_analyze.py stats trace.trc --export-csv stats.csv
```

**Statistical Post-Processing** *(Implemented)*
- ✅ Call graph generation (text tree, GraphViz DOT)
- ✅ Performance regression detection (baseline vs current comparison)
- ✅ Trace diff (execution path comparison)
- ✅ Filtering support for all post-processing commands
- ✅ CSV/JSON export for all features
- ✅ 12 comprehensive tests across 3 modules, all passing
- See Statistical Post-Processing section above

**Example:**
```bash
# Call graph
python tools/trc_analyze.py callgraph trace.trc --format dot -o callgraph.dot

# Regression detection
python tools/trc_analyze.py compare baseline.trc current.trc --threshold 10

# Trace diff
python tools/trc_analyze.py diff trace_a.trc trace_b.trc
```

**Directory Organization & Batch Processing** *(Implemented - v0.8.0-alpha)*
- ✅ .trc file extension for trace files
- ✅ Configurable output directories (flat/date/session layouts)
- ✅ Automatic directory creation
- ✅ Session auto-increment
- ✅ Python analyzer processes directories of traces
- ✅ Recursive subdirectory search
- ✅ Chronological/name/size file sorting
- See Binary Dump Format section above

**Async Immediate Mode** *(Implemented - v0.9.0-alpha)*
- ✅ Replaced synchronous I/O with async queue + background writer
- ✅ 10-20x performance improvement over old sync mode
- ✅ Non-blocking event enqueue (~5µs overhead)
- ✅ Configurable flush interval (default: 1ms)
- ✅ Force-flush API: `flush_immediate_queue()`
- ✅ Hybrid mode also uses async for immediate output
- ✅ 6 comprehensive tests, all passing
- ✅ Performance benchmarking example
- See Async Immediate Mode Details section above

**Example:**
```cpp
trace::config.mode = trace::TracingMode::Immediate;
trace::config.immediate_flush_interval_ms = 1;  // 1ms latency
TRACE_SCOPE();  // Non-blocking, ~5µs overhead
trace::flush_immediate_queue();  // Force flush if needed
```

### Near-Term Features

### Medium-Term Goals

**VS Code Extension**
- Syntax highlighting for trace macros
- Quick actions: "Add TRACE_SCOPE to function"
- View trace output in integrated terminal
- Jump to function from trace output
- Toggle tracing on/off per file

**Chrome Tracing Format Export**
- Export to chrome://tracing JSON format
- Timeline visualization
- Thread swimlanes
- Flame graph view

**Enhanced Query/Analysis**
- SQL-like query syntax for trace files
- Saved query templates
- Time-series analysis (slice by time ranges)
- Historical tracking with SQLite database
- Trend detection and anomaly identification

### Future Considerations
- Compression for binary dumps (zlib/zstd)
- Native OS tracing integration (ETW/dtrace/perf)
- SIMD optimizations for high-frequency tracing
- Lock-free queue for async immediate mode (further optimization)

### Contributing

Contributions welcome! If you'd like to work on any roadmap items or have other ideas:
- Open an issue to discuss the feature
- Tests should pass (C++ and Python)
- Code follows existing style
- Documentation updated for new features

