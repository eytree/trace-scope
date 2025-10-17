# trace-scope

A lightweight, header-only C++ library for function-scope tracing with per-thread ring buffers.

## Features

- **TRACE_SCOPE()**: Automatic enter/exit recording with depth indentation and duration
- **TRACE_MSG(fmt, ...)**: Formatted message logging at current depth
- **Per-thread ring buffers**: Lock-free writes, thread-safe flushing
- **Immediate mode**: Real-time output for long-running processes
- **Binary dump**: Compact binary format with Python pretty-printer
- **DLL-safe**: Optional shared state across DLL boundaries
- **Configurable output**: Timestamps, thread IDs, file/line info, function names
- **Header-only**: Just include and use (default mode)

## Quick Start

```cpp
#include <trace_scope.hpp>

void foo() {
    TRACE_SCOPE();
    TRACE_MSG("Processing data");
    // ... work ...
}

int main() {
    TRACE_SCOPE();
    foo();
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

// Advanced options
trace::config.immediate_mode = false;       // Real-time output (default: false, opt-in)
trace::config.auto_flush_at_exit = false;   // Auto-flush on scope exit (default: false, opt-in)
```

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

For compact storage and analysis:

```cpp
trace::dump_binary("trace.bin");
```

Pretty-print with included Python tool:
```bash
python tools/trc_pretty.py trace.bin
```

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
- `example_basic.cpp`: Basic usage with multi-threaded tracing
- `example_dll_shared.cpp`: Header-only DLL state sharing demonstration
- `src/trace_scope_impl.cpp`: Advanced DLL compilation-based sharing template

See `tests/` directory:
- `test_trace.cpp`: Basic functionality tests
- `test_comprehensive.cpp`: Extensive test suite covering all features

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

- C++17 or later
- CMake 3.16+ (for building tests/examples)
- Windows: MSVC 2019+, MinGW, or Clang
- Linux/Mac: GCC 7+, Clang 5+

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

## Contributing

Contributions welcome! Please ensure:
- Tests pass (`test_comprehensive`)
- Code follows existing style
- Documentation updated for new features
