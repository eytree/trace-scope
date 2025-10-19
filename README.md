# trace-scope

A lightweight, header-only C++ library for function-scope tracing with per-thread ring buffers.

## Features

- **TRACE_SCOPE()**: Automatic enter/exit recording with depth indentation and duration
- **TRACE_MSG(fmt, ...)**: Printf-style formatted message logging
- **TRACE_LOG**: Stream-based logging (C++ iostream style, drop-in for stream macros)
- **Per-thread ring buffers**: Lock-free writes, thread-safe flushing
- **Immediate mode**: Real-time output for long-running processes
- **Binary dump**: Compact binary format with Python pretty-printer
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
- Best performance, lowest overhead
- Events lost on crash if not flushed

**Immediate Mode**:
- Events printed immediately, no buffering
- Useful for crash scenarios or real-time monitoring
- Higher overhead (file I/O on every event)
- Thread-safe but serialized (mutex-protected)

**Hybrid Mode**:
- Events both buffered AND printed immediately
- Best of both worlds: real-time visibility + history
- Auto-flush when buffer reaches threshold
- Separate output streams possible (see Hybrid Mode section below)
- No post-processing needed (no flush required)

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

For compact storage and post-processing:

```cpp
trace::dump_binary("trace.bin");
```

### Python Pretty-Printer

The included `tools/trc_pretty.py` tool parses and displays binary trace files:

```bash
python tools/trc_pretty.py trace.bin
```

**Features:**
- Parses TRCLOG10 binary format
- Auto-scaling duration units (ns/us/ms/s)
- Visual indent markers (`|`) for call depth
- Timestamps and thread IDs
- Full file paths and line numbers

**Output format:**
```
[timestamp] (thread_id) | | -> function_name (file:line)
[timestamp] (thread_id) | | | - message text (file:line)
[timestamp] (thread_id) | | <- function_name  [duration] (file:line)
```

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

**`trc_pretty.py`** - Binary trace file pretty-printer
- Parses and displays TRCLOG10 binary dumps
- Usage: `python tools/trc_pretty.py trace.bin`
- See "Binary Dump Format" section for details

**`trace_instrument.py`** - Automatic code instrumentation
- Adds/removes TRACE_SCOPE() from C++ files
- Usage: `python tools/trace_instrument.py add file.cpp`
- Usage: `python tools/trace_instrument.py remove file.cpp`
- Tests: `python tools/test_trace_instrument.py`
- See "Automatic Instrumentation Tool" section for details

**Python Tool Tests:**
- `test_trace_instrument.py` - Unit tests for instrumentation tool (11 tests)
- `test_trc_pretty.py` - Unit tests for binary parser (10 tests)
- Both require Python 3.6+
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
- Python 3.6+ (for trc_pretty.py, trace_instrument.py)
- No external dependencies (uses only standard library)

## License

See LICENSE file.

## Performance Considerations

**Buffered mode (default)**:
- Minimal overhead (~10-50ns per trace call)
- Lock-free writes to per-thread buffer
- Ring buffer wraps on overflow (oldest events lost)
- Flush when convenient

**Immediate mode**:
- Higher overhead (file I/O per event)
- Mutex serialization across threads
- No buffering, no data loss on crash
- Real-time visibility

**Recommendations**:
- Use buffered mode for performance-critical code
- Use immediate mode for debugging crashes or real-time monitoring
- Disable tracing in release builds (`#define TRACE_ENABLED 0`)

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

### Near-Term Features

**Performance Metrics & Analysis**
- Per-function call count tracking
- Min/max/average duration statistics
- Hotspot identification (most time spent)
- Call frequency analysis
- Export metrics summary (JSON/CSV)

**Example:**
```cpp
trace::metrics::enable();
// ... run code ...
trace::metrics::print_summary();  // Shows top 10 slowest functions
```

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

**Statistical Post-Processing (Python tools)**
- Call graph generation (GraphViz)
- Performance regression detection
- Trace diff between runs
- Filter/query trace files

### Future Considerations
- Asynchronous I/O for immediate mode
- Compression for binary dumps (zlib/zstd)
- Native OS tracing integration (ETW/dtrace/perf)
- SIMD optimizations for high-frequency tracing

### Contributing

Contributions welcome! If you'd like to work on any roadmap items or have other ideas:
- Open an issue to discuss the feature
- Tests should pass (C++ and Python)
- Code follows existing style
- Documentation updated for new features

