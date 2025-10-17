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
example.cpp:   8 foo                  <- foo  [123.45 µs]
example.cpp:  13 main                 -> main
example.cpp:  14 main                 <- main  [234.56 µs]
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

## DLL Boundary Support

By default, trace_scope is header-only. Each DLL gets its own copy of trace state.

To share trace state across DLLs:

1. **Create implementation file** (see `src/trace_scope_impl.cpp` template):
```cpp
#define TRACE_SCOPE_IMPLEMENTATION
#include <trace_scope.hpp>
```

2. **Define `TRACE_SCOPE_SHARED`** when compiling all files:
```bash
# Windows (MSVC)
cl /DTRACE_SCOPE_SHARED /DTRACE_SCOPE_IMPLEMENTATION src/trace_scope_impl.cpp main.cpp

# Linux/Mac (GCC/Clang)
g++ -DTRACE_SCOPE_SHARED -DTRACE_SCOPE_IMPLEMENTATION src/trace_scope_impl.cpp main.cpp
```

3. **In other files**, just include normally:
```cpp
#include <trace_scope.hpp>  // Will use shared state
```

**Note**: Use TRACE_SCOPE_IMPLEMENTATION in only ONE .cpp file per executable/DLL.

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
- `< 1 µs`: nanoseconds (e.g., `[123 ns]`)
- `< 1 ms`: microseconds (e.g., `[45.67 µs]`)
- `< 1 s`: milliseconds (e.g., `[123.45 ms]`)
- `≥ 1 s`: seconds (e.g., `[2.345 s]`)

## Examples

See `examples/` directory:
- `example_basic.cpp`: Basic usage with threads
- `src/trace_scope_impl.cpp`: DLL-safe implementation template

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
