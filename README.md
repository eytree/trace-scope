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

// Advanced options
trace::config.immediate_mode = false;       // Real-time output (default: false, opt-in)
trace::config.auto_flush_at_exit = false;   // Auto-flush on scope exit (default: false, opt-in)
trace::config.use_double_buffering = false; // Enable double-buffering (default: false, opt-in)
```

#### Double-Buffering Mode

For extremely high-frequency tracing scenarios where flush operations are called frequently:

```cpp
trace::config.use_double_buffering = true;  // Enable double-buffering
```

**Benefits:**
- **Eliminates race conditions** during flush operations
- **Zero disruption**: Write to one buffer while flushing the other
- **Safe concurrent operations**: No blocking between writes and flushes
- **Better performance** in high-frequency scenarios (millions of events/sec)

**Trade-offs:**
- **2x memory usage** per thread (~4MB per thread with default settings)
- **Slightly more complex** implementation

**When to use:**
- Generating millions of trace events per second
- Frequent flush operations (e.g., every 10ms)
- Multiple threads with high-frequency tracing
- When you need guaranteed race-free flush operations

**Example:**
```cpp
// Enable double-buffering for high-frequency tracing
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

### ANSI Color Support

For easier visual tracking of call depths, enable ANSI color-coding:

```cpp
trace::config.colorize_depth = true;  // Enable depth-based coloring
```

Each call depth level gets a different color from a smooth 30-color gradient:
- **Depths 1-8:** Green shades (light to dark green)
- **Depths 9-12:** Yellow-green transition
- **Depths 13-18:** Yellow to orange transition
- **Depths 19-24:** Orange to red transition
- **Depths 25-30:** Deep red shades

The gradient provides up to 30 distinct colors, cycling after that. Colors are chosen to be clearly visible and avoid hard-to-read combinations.

**Example:**
```cpp
void foo() {
    TRACE_SCOPE();  // Depth 1 = Light Green
    bar();
}

void bar() {
    TRACE_SCOPE();  // Depth 2 = Green
    baz();
}

void baz() {
    TRACE_SCOPE();  // Depth 3 = Darker Green
    // ... continues through gradient as depth increases
}
```

**Notes:**
- Colors only work in ANSI-compatible terminals (most modern terminals)
- Windows Terminal, Linux terminals, macOS Terminal all support ANSI
- Old Windows cmd.exe may not display colors correctly (use Windows Terminal instead)
- Works with all marker styles (ASCII, Unicode, box-drawing)
- See `examples/example_colors.cpp` for a demonstration

## Immediate Mode

For long-running processes or real-time logging, enable immediate mode:

```cpp
trace::config.immediate_mode = true;  // Output printed immediately, no ring buffer
```

This bypasses the ring buffer and prints events directly. Useful when:
- Process may crash and you need immediate output
- Real-time monitoring during development
- Memory-constrained environments

Trade-offs:
- Slower than buffered mode (file I/O on every event)
- Thread-safe but serialized (mutex-protected output)
- No post-processing needed (no flush required)

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

### Recommended Solution: External State Injection

**For large codebases where you cannot control which files are compiled**, use the header-only external state injection:

1. **Create shared state in your main executable** (or a common header):
```cpp
// In main.cpp or a shared header included by all DLLs:
namespace {
    trace::Config g_trace_config;
    trace::Registry g_trace_registry;
}

int main() {
    // Initialize shared state BEFORE any tracing occurs
    trace::set_external_state(&g_trace_config, &g_trace_registry);
    
    // Configure as needed
    g_trace_config.out = std::fopen("trace.log", "w");
    
    // Now all DLLs will share the same trace state!
    // ...
}
```

2. **All DLLs automatically use the shared state** - no changes needed!

**That's it!** Completely header-only, no compilation requirements, works across any number of DLLs.

### How It Works

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

Control buffer sizes at compile time:

```cpp
#define TRACE_ENABLED 1        // Enable/disable all tracing (default: 1)
#define TRACE_RING_CAP 4096    // Events per thread (default: 4096)
#define TRACE_MSG_CAP 192      // Max message size (default: 192)
#define TRACE_DEPTH_MAX 512    // Max call depth tracked (default: 512)

#include <trace_scope.hpp>
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

### Immediate Priority (Next Release)

**Hybrid Buffered + Immediate Mode**
- Support simultaneous buffered and immediate output
- Auto-flush ring buffer when near capacity (e.g., 90% full)
- Prevents data loss while maintaining performance
- Optional file rotation when buffer wraps

**Use case:** Long-running processes where you want both real-time visibility and complete history without data loss.

### Near-Term Features

**Filtering & Selective Tracing**
- Filter by function name patterns (regex)
- Filter by file/path patterns
- Filter by depth range
- Enable/disable tracing dynamically at runtime
- Thread-specific filtering

**Example:**
```cpp
trace::filter::exclude_functions(".*test.*");  // Skip test functions
trace::filter::include_files("src/core/.*");   // Only trace core files
trace::filter::max_depth(10);                  // Limit depth
```

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

