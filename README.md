# trace-scope

Header-only C++ tracing utility for **function scope** and **inline messages** with:
- per-thread **lock-free ring buffers**
- **depth-aligned** indentation
- **timestamps**, **thread id**, and **durations**
- `TRACE_SCOPE()` for enter/exit
- `TRACE_MSG(fmt, ...)` for inline messages
- `flush_all()` for pretty text output, and `dump_binary()` + `tools/trc_pretty.py` for post-processing at scale

## Quick start

```bash
git clone <your-repo-url> trace-scope
cd trace-scope
cmake -S . -B build -G Ninja -DTRACE_ENABLED=ON -DBUILD_TESTS=ON
cmake --build build --config Release
./build/example_basic    # or .\build\example_basic.exe on Windows
./build/test_trace
```

### Use in your project

```cpp
#define TRACE_ENABLED 1
#include <trace_scope.hpp>

void work() {
    TRACE_SCOPE();
    TRACE_MSG("hello x=%d", 42);
}

int main() {
    TRACE_SCOPE();
    trace::config.out = fopen("trace.log", "w"); // optional redirect
    work();
    trace::flush_all();
    trace::dump_binary("trace.bin");             // optional
}
```

### CMake

Add as a subdirectory or copy `include/trace_scope.hpp` into your include path.
```cmake
add_subdirectory(trace-scope)
target_link_libraries(your_target PRIVATE trace_scope)
```

## Binary dump & pretty printer

To avoid formatting overhead during runs with huge traces:

1. In C++:
```cpp
trace::dump_binary("trace.bin");
```
2. Later:
```bash
python3 tools/trc_pretty.py trace.bin > trace.txt
```

`trc_pretty.py` reproduces the pretty text output (timestamps, thread ids, indentation, durations).

## Design notes

- **thread_local rings**: Each thread writes to its own ring (`TRACE_RING_CAP` entries, default 4096). No locks on the hot path.
- **Depth & durations**: `TRACE_SCOPE()` adjusts a per-thread depth counter and a small `start_stack` (default `TRACE_DEPTH_MAX=512`) to compute duration on exit.
- **Buffered messages**: `TRACE_MSG` emits a `Msg` event (file/line captured automatically) at the current depth, without changing depth.
- **Flushing**: `flush_all()` sorts per-thread rings chronologically **within each thread** and prints to `trace::config.out`. For cross-thread time ordering, prefer the binary dump and post-process (or push to a timeline tool).
- **Toggles**: `TRACE_ENABLED=0` compiles out all instrumentation. Runtime flags in `trace::config` let you show/hide timestamps, thread ids, file:line, timings, etc.

## Options

You can define these at compile time (e.g., `-DTRACE_RING_CAP=8192`):

- `TRACE_ENABLED` (default 1)
- `TRACE_RING_CAP` (default 4096)
- `TRACE_MSG_CAP` (default 192)
- `TRACE_DEPTH_MAX` (default 512)

## Windows/MSVC tips

- No special flags needed for this header-only approach.
- If you combine with `_penter/_pexit` (`/Gh /GH`), mark tracer TU as non-instrumented or compile selectively to avoid recursion.

## License

MIT — see [LICENSE](LICENSE).
