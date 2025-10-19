# trace-scope Development History

This document tracks major features, design decisions, and implementation milestones.

---

## October 19, 2025

### Python Tool Sync - Binary Format v2 + Filtering + Colors
**Commit:** `4654d33`  
**Breaking Change:** Binary format version bumped from 1 to 2

**Problem:** Python post-processing tool (`trc_pretty.py`) was out of sync with C++ tracer:
- No filtering capabilities
- No color output
- Missing `color_offset` field for thread-aware colors
- Limited functionality compared to runtime

**Solution:**
- Bumped binary format to version 2 with `color_offset` field
- Complete rewrite of `trc_pretty.py` (~290 lines)
- Added wildcard pattern matching (matches C++ implementation)
- Added EventFilter class with full filtering support
- Added thread-aware ANSI color output
- Added comprehensive command-line options

**Binary Format Changes:**
- Version 1: `type(1) + tid(4) + ts_ns(8) + depth(4) + dur_ns(8) + strings + line(4)`
- Version 2: `type(1) + tid(4) + color_offset(1) + ts_ns(8) + depth(4) + dur_ns(8) + strings + line(4)`
- Python tool supports both versions (backward compatible parsing)

**Python Tool Features:**
```bash
# Thread-aware colors
python trc_pretty.py trace.bin --color

# Function filtering
python trc_pretty.py trace.bin --filter-function "core_*" --exclude-function "*_test"

# File filtering
python trc_pretty.py trace.bin --filter-file "src/core/*" --exclude-file "*/test/*"

# Thread filtering
python trc_pretty.py trace.bin --filter-thread 0x1234 --exclude-thread 0x5678

# Depth limiting
python trc_pretty.py trace.bin --max-depth 10

# Complex combinations
python trc_pretty.py trace.bin --color --max-depth 8 \
    --filter-function "my_namespace::*" \
    --exclude-function "debug_*"
```

**Design Decisions:**
- **Why rewrite vs patch?** Cleaner architecture, better maintainability
- **Why argparse?** Standard Python library, comprehensive option handling
- **Why match C++ filtering?** Consistent behavior between runtime and post-processing
- **Why support version 1?** Graceful handling of old trace files (color_offset defaults to 0)

**Implementation:**
- `wildcard_match()` - Converts `*` wildcards to regex (same logic as C++)
- `EventFilter` class - Matches C++ filter logic exactly (exclude wins, depth check, etc.)
- `get_color()` - Thread-aware ANSI colors using `(depth + color_offset) % 8`
- `process_trace()` - Main loop with filter application and statistics

**Testing:**
- Created `test_trc_pretty_v2.py` with 7 comprehensive tests
- All tests pass: wildcard matching, function filters, file filters, thread filters, exclude priority
- Created `test_python_tool.cpp` to generate test binaries
- Verified filtering, colors, and statistics work correctly
- Tested with version 2 binaries (color_offset included)

**Statistics Output:**
```
# Binary format version: 2
# Processed 100 events, displayed 45
# Filtered out 55 events (55.0%)
```

**Files:**
- `include/trace-scope/trace_scope.hpp` (+16 lines) - Binary format v2, updated comments
- `tools/trc_pretty.py` (rewrite, +185 lines, -92 old) - Complete feature parity
- `tools/test_trc_pretty_v2.py` (145 lines) - Comprehensive testing
- `examples/test_python_tool.cpp` (50 lines) - Test binary generator
- `README.md` (+55 lines) - Python tool documentation with options table

**Benefits:**
- Python tool now feature-complete with C++ runtime
- Post-process existing traces with different filters
- Thread-aware colors match runtime output exactly
- Easy integration into CI/analysis pipelines
- No need to re-run programs to apply different filters

---

### Thread-Aware Color Coding
**Commit:** `89a6b86`

**Feature:** Automatic thread-aware color offsetting for multi-threaded tracing visualization.

**Problem:** In multi-threaded applications, all threads used the same color scheme based only on depth, making it hard to visually distinguish which thread produced each trace line.

**Solution:**
- Added `color_offset` field to Ring struct (calculated as `tid % 8`)
- Added `color_offset` field to Event struct (1 byte)
- Updated print_event() to use `(depth + color_offset) % 8` for color selection
- Simplified color scheme from 256-color gradient to 8-color system

**Implementation:**
- Ring constructor now calculates thread-specific color offset
- Each thread gets offset 0-7 based on its thread ID hash
- Color index = (event depth + thread offset) % 8
- Uses 8 standard ANSI colors: Red, Green, Yellow, Blue, Magenta, Cyan, White, Bright Red

**Benefits:**
- **Visual thread distinction**: Each thread uses a different color pattern
- **Depth info preserved**: Colors still cycle as depth increases  
- **Zero runtime overhead**: Offset calculated once per thread in constructor
- **Automatic**: No configuration needed, works out of the box
- **Backward compatible**: Old traces read offset as 0 (no offset)

**Example:**
```
Main Thread (offset=0):  Red → Green → Yellow → Blue...
Worker Thread (offset=3): Blue → Magenta → Cyan → White...
```

**Design Decisions:**
- **Why 8 colors instead of 256?** Simpler, clearer visual distinction, sufficient for most use cases
- **Why offset instead of solid color?** Preserves both depth and thread information in one visual
- **Why store in Event?** Needed for post-flush output and binary format replay
- **Why tid % 8?** Simple, deterministic, uses all 8 colors evenly

**Performance Impact:**
- +1 byte per Event struct
- +1 byte per Ring struct
- Zero runtime overhead (offset calculated once in Ring constructor)
- No additional comparisons in hot path

**Files:**
- `include/trace-scope/trace_scope.hpp` (+18 lines, -21 simplified gradient code)
- `examples/example_thread_colors.cpp` (100 lines) - multi-threaded demonstration
- `README.md` (+11 lines, -8 old gradient docs)
- `examples/CMakeLists.txt` (+3 lines)

**Testing:**
- All 57 existing tests pass
- Manual testing with example_thread_colors shows distinct color patterns per thread
- Verified depth still affects color within each thread

---

### Filtering and Selective Tracing
**Commit:** `f992566`

**Feature:** Runtime filtering to focus tracing on specific functions, files, or depth ranges.

**Implementation:**
- Simple wildcard pattern matching (`*` matches zero or more characters)
- Function filters: include/exclude patterns with wildcard support
- File filters: include/exclude patterns for file paths
- Depth filter: max_depth to limit call depth (-1 = unlimited)
- Filter API: `filter_include_function()`, `filter_exclude_function()`, etc.
- INI configuration: `[filter]` section with multiple pattern support
- 22 comprehensive tests covering all filter scenarios
- Example demonstrating all filter types

**Design Decisions:**
- **Why wildcards vs regex?** Simpler to implement (~50 lines), sufficient for 99% of use cases, no external dependencies
- **Why both include and exclude?** Maximum flexibility - can trace "everything except X" or "only X except Y"
- **Why exclude wins?** Clear precedence rule prevents ambiguity
- **Why max_depth only?** Simpler than min/max range, covers the main use case (limiting recursion spam)
- **Filter at trace time vs output time?** At trace time saves memory (filtered events never written to buffer)

**Filter Logic:**
1. Check depth filter (if max_depth set and exceeded, filter out)
2. Check function filters:
   - If exclude list matches → filter out (exclude wins)
   - If include list is not empty and doesn't match → filter out
3. Check file filters (same logic as function filters)
4. Pass all checks → trace the event

**Pattern Matching Algorithm:**
- Recursive wildcard matching for simplicity
- Case-sensitive matching
- Examples: `test_*`, `*_test`, `*mid*`, `namespace::*`
- O(n*m) worst case, but typically very fast (small patterns, early exits)

**API Functions:**
```cpp
trace::filter_include_function("core_*");      // Only trace core functions
trace::filter_exclude_function("*_test");      // Never trace test functions
trace::filter_include_file("src/networking/*"); // Only trace networking files
trace::filter_exclude_file("*/test/*");        // Never trace test directories
trace::filter_set_max_depth(10);               // Limit to depth 10
trace::filter_clear();                         // Clear all filters
```

**INI Configuration:**
```ini
[filter]
include_function = core_*
include_function = important_*
exclude_function = *_test
exclude_function = debug_*
include_file = src/core/*
exclude_file = */test/*
max_depth = 15
```

**Use Cases:**
- Focus on specific module: exclude everything except core functionality
- Exclude test code: trace production code only
- Limit deep recursion: prevent stack overflow scenarios from filling logs
- Debug specific subsystem: include only networking or database code

**Performance Impact:**
- Minimal overhead: O(patterns) string comparisons per TRACE_SCOPE() call
- Typical use: < 10 patterns, negligible impact
- Filtered events never written to ring buffer (saves memory)
- Early return on filter match (no event allocation)

**Implementation Details:**
- ~200 lines of filter code in `trace_scope.hpp`
- `filter_utils` namespace for wildcard matching and filter logic
- Depth tracking maintained even for filtered events (preserves nesting)
- Filter check wrapped in `#if TRACE_ENABLED` for compile-time removal

**Edge Cases Handled:**
- Null function pointers (for TRACE_MSG) - only check file filter
- Empty filter lists - trace everything (default behavior)
- Multiple patterns accumulate (multiple include_function lines in INI)
- Filter that matches nothing - no traces output (expected behavior)

**Files:** 
- `include/trace-scope/trace_scope.hpp` (+243 lines)
- `examples/example_filtering.cpp` (206 lines)
- `examples/filter_config.ini` (44 lines)
- `tests/test_filtering.cpp` (323 lines)
- `README.md` (+130 lines documentation)

**Testing:**
- 22 tests: wildcard matching (7), function filters (6), file filters (4), depth (2), integration (3)
- 100% pass rate
- Total test suite: 79 tests across all executables

**Benefits:**
- Focus debugging efforts on relevant code
- Reduce trace output volume by 10-100x
- Prevent log spam from deep recursion
- Runtime configurable - no recompilation needed
- Zero external dependencies

---

### Build Scripts for Windows
**Commit:** `0abb4ef`

**Implementation:**
- Added 4 build scripts (batch and PowerShell) for Ninja+MSVC and Ninja+Clang
- Scripts support clean builds, running tests, and enabling double-buffering
- Separate build directories to avoid conflicts with existing builds

**Design Decisions:**
- Both .bat and .ps1 versions for maximum compatibility
- PowerShell versions have enhanced features (-Clean, -Test, -DoubleBuf flags)
- Auto-detection of compilers and Visual Studio installation
- Color-coded output for better UX

**Files:** `build_ninja_msvc.bat`, `build_ninja_clang.bat`, `build_ninja_msvc.ps1`, `build_ninja_clang.ps1`

---

### Compile-Time Double-Buffering
**Commit:** `bdeb7b1`  
**Breaking Change:** Double-buffering now requires compile-time flag

**Problem:** Always allocating 2 buffers wasted ~1.2MB per thread (50%) when double-buffering unused.

**Solution:**
- Added `TRACE_DOUBLE_BUFFER` compile-time macro (default: OFF)
- Ring struct uses `TRACE_NUM_BUFFERS` (1 or 2) instead of hardcoded 2
- Conditional compilation of double-buffer tests and examples
- Runtime warning if `use_double_buffering=true` without compile support

**Design Decisions:**
- **Why compile-time?** Most users don't need double-buffering; this saves memory by default
- **Why not always on?** 50% memory overhead is significant for a rarely-used feature
- **Migration:** Users needing it just add `-DTRACE_DOUBLE_BUFFER=ON` to CMake

**Memory Impact:** Saves ~1.2MB per thread by default

**Files:** `include/trace-scope/trace_scope.hpp`, `CMakeLists.txt`, examples/tests CMakeLists, README.md

---

### TracingMode Enum Refactor
**Commit:** `227d405`  
**Breaking Change:** Removed `immediate_mode` and `hybrid_mode` boolean flags

**Problem:** Two boolean flags could be in conflicting states (both true), leading to undefined behavior.

**Solution:**
- Replaced with single `TracingMode` enum: `Buffered`, `Immediate`, `Hybrid`
- Updated all code to use `config.mode == TracingMode::X`
- Updated INI parser to accept `mode = buffered|immediate|hybrid`

**Design Decisions:**
- **Why enum?** Type-safe, prevents invalid configurations, clearer intent
- **Why break compatibility?** Clean API more important than temporary backward compat
- **INI format:** Single `mode` key instead of two boolean keys - simpler, clearer

**Files:** `include/trace-scope/trace_scope.hpp`, all examples, all tests, INI files, README.md

**Benefits:** Impossible to set conflicting modes, clearer API, better code readability

---

## October 18-19, 2025

### INI Configuration File Support
**Commit:** `95a9dd2`

**Feature:** Load all trace configuration from external INI file.

**Implementation:**
- Built-in INI parser (~200 lines, zero dependencies)
- `trace::load_config(path)` function
- `TRACE_SETUP_DLL_SHARED_WITH_CONFIG(path)` macro variant
- Supports all config options across 5 INI sections

**Design Decisions:**
- **Why INI over YAML?** Simpler parsing (~200 lines vs ~500 lines), all config is flat
- **Why not JSON?** Harder to hand-edit, less human-friendly for config files
- **Error handling:** Warn on errors but continue (graceful degradation)
- **String markers:** Support both quoted and unquoted values

**INI Sections:** `[output]`, `[display]`, `[formatting]`, `[markers]`, `[modes]`

**Tests:** 11 comprehensive tests validating parser, all data types, error handling

**Files:** `include/trace-scope/trace_scope.hpp` (+250 lines), `examples/trace_config.ini`, `examples/example_config_file.cpp`, test suite

---

### DLL Setup Macro Simplification
**Commit:** `a66674b`

**Problem:** DLL state sharing required 20+ lines of boilerplate code.

**Solution:**
- `TRACE_SETUP_DLL_SHARED()` macro - RAII guard for automatic setup/cleanup
- `TRACE_SETUP_DLL_SHARED_WITH_CONFIG(path)` - DLL setup + config loading in one line

**Design Decisions:**
- **Why RAII?** Automatic cleanup, exception-safe, no manual flush needed
- **Why macro over function?** Creates static instances with proper lifetime
- **Config access:** Use `trace::get_config()` to access shared configuration

**Before:** 20+ lines of namespace, static variables, constructor/destructor  
**After:** 1 line macro call

**Files:** `include/trace-scope/trace_scope.hpp`, `examples/example_dll_shared.cpp`, README.md

---

### Roadmap Update
**Commit:** `a82d81a`

**Update:** Moved completed features to "Recently Completed" section.

**Completed Features Highlighted:**
- Hybrid buffered + immediate mode
- Double-buffering mode
- Comprehensive test framework

**Rationale:** Clear communication of what's production-ready vs planned

---

### Test Framework with Selective Execution
**Commits:** `aef57a1`, `827f74e`, `d38e673`

**Feature:** Lightweight, zero-dependency test framework for selective test execution.

**Implementation:**
- Header-only test framework (`test_framework.hpp`)
- `TEST()` macro for automatic registration
- `TEST_ASSERT()`, `TEST_ASSERT_EQ()`, `TEST_ASSERT_NE()` macros
- CLI support: `--list`, `--help`, filter by substring
- `run_tests(argc, argv)` runner function

**Design Decisions:**
- **Why custom framework?** Zero dependencies, tailored for trace-scope needs
- **Why not Catch2/GTest?** Against project philosophy (no external dependencies)
- **Registration:** Static constructors (same pattern as existing test_comprehensive)
- **Output format:** Clear pass/fail with test counts

**Migration:** All tests migrated to use new framework (test_comprehensive, test_double_buffer, test_trace, test_binary_format)

**Test Count:** 28 tests across 5 test executables, 100% pass rate

**Files:** `tests/test_framework.hpp` (335 lines), all test files, README.md

---

### Critical Bug Fix: Thread-Local Ring Cleanup
**Commit:** `abe741c`

**Problem:** When worker threads exited, their thread_local Ring was destroyed but pointer remained in global registry, causing crashes when `flush_all()` accessed freed memory.

**Solution:**
- Added `Registry::remove(Ring*)` method
- Added `Ring::~Ring()` destructor to unregister from global registry
- Forward declaration pattern to resolve dependency ordering

**Impact:** Fixed segmentation faults in multi-threaded scenarios

**Trade-off:** Events from exited threads lost unless flushed before thread termination (documented behavior)

**Files:** `include/trace-scope/trace_scope.hpp`

---

## October 18, 2025

### Double-Buffering Implementation
**Commit:** `8e10b19`

**Feature:** Optional double-buffering for high-frequency tracing scenarios.

**Problem:** In high-frequency scenarios with frequent flushes, race conditions possible when flush_ring() reads buffer while write() writes to it.

**Solution:**
- Dual ring buffers per thread: write to one while flushing the other
- Atomic buffer swap on flush
- `Config::use_double_buffering` runtime flag

**Design Decisions:**
- **Why runtime flag?** Allows switching modes without recompilation
- **Why two buffers?** Eliminates all race conditions during flush
- **Memory cost:** Acceptable (2x) for high-frequency scenarios
- **Later optimized:** Made compile-time optional to save memory (see bdeb7b1)

**Use Cases:** Millions of events/sec, frequent flushes (every 10ms), race-free guarantees

**Tests:** 5 comprehensive tests (functional, ordering, correctness, swap verification, stress)

**Files:** `include/trace-scope/trace_scope.hpp` (Ring struct changes), `example_double_buffer.cpp`, `test_double_buffer.cpp`, README.md

---

### Hybrid Mode Console Output Fix
**Commit:** `cd62510`

**Problem:** Hybrid mode example wasn't demonstrating real-time console output.

**Solution:**
- Set `immediate_out = stdout` (console)
- Set `out = file` (buffered history)

**Rationale:** Proper demonstration of hybrid mode's dual-stream capability

**Files:** `examples/example_hybrid.cpp`

---

### Project Structure Reorganization
**Commit:** `85bc9ab`

**Major Refactor:** Improved project structure for better usability.

**Changes:**
1. Moved header: `include/trace_scope.hpp` → `include/trace-scope/trace_scope.hpp`
2. Created modular CMake structure (examples/ and tests/ have own CMakeLists.txt)
3. Updated all includes to use `<trace-scope/trace_scope.hpp>`

**Design Decisions:**
- **Why subdirectory?** Clearer import path, matches common C++ practices
- **Why modular CMake?** Better organization, easier to maintain
- **Include style:** Angle brackets `<>` emphasize it's a library header

**Benefits:**
- Professional import path: `#include <trace-scope/trace_scope.hpp>`
- Cleaner project structure
- Easier to integrate as submodule or dependency

**Files:** Moved header, `CMakeLists.txt` (modular), `examples/CMakeLists.txt` (new), `tests/CMakeLists.txt` (new), all source files

---

## Design Philosophy

Throughout development, key principles maintained:

1. **Zero External Dependencies**
   - All features implemented using only C++17 standard library
   - Custom INI parser, test framework, etc.
   - Rationale: Easy integration, no dependency hell

2. **Header-Only Default**
   - Main library is pure header-only
   - DLL support available but optional
   - Rationale: Simplest possible integration

3. **Performance-Conscious**
   - Lock-free writes, compile-time feature flags
   - Memory optimizations (compile-time double-buffering)
   - Rationale: Tracing shouldn't slow down production code

4. **User-Friendly API**
   - Simple macros: `TRACE_SCOPE()`, `TRACE_MSG()`, `TRACE_LOG`
   - One-line setup macros for complex scenarios
   - Rationale: Low barrier to adoption

5. **Type Safety**
   - Enums over booleans where appropriate
   - Compile-time checks prevent runtime errors
   - Rationale: Catch errors early, clearer intent

6. **Graceful Degradation**
   - Missing config files use defaults
   - Parse errors skip bad lines and continue
   - Runtime checks for compile-time features
   - Rationale: Robust in production

---

## Key Metrics

- **Total Development Time:** Multiple sessions over 2 days
- **Commits This Session:** 12
- **Lines Added:** ~1,600+
- **Test Coverage:** 39 tests (28 framework tests + 11 config tests)
- **Examples:** 7 comprehensive examples
- **Memory Saved:** ~1.2MB per thread by default (double-buffer opt-in)
- **External Dependencies:** 0

---

## Future Considerations

See [Roadmap](README.md#roadmap) for planned features:
- Filtering & selective tracing
- Performance metrics & analysis
- VS Code extension
- Chrome tracing format export
- Statistical post-processing tools

---

*Last Updated: October 19, 2025*

