# Release Notes - trace-scope v0.8.0-alpha

## Overview

Version 0.8.0-alpha introduces major improvements to trace file organization and batch processing capabilities while establishing a consistent versioning system across all tools.

## Breaking Changes

⚠️ **File Extension Change**: Trace files now use `.trc` extension instead of `.bin`
- All new trace files will be generated with `.trc` extension
- Python analyzer no longer supports `.bin` files
- This is an alpha release - expect breaking changes before 1.0

## New Features

### 1. Version Management System
- **Single Source of Truth**: `VERSION` file at project root (0.8.0-alpha)
- **C++ Header**: Version constants (`TRACE_SCOPE_VERSION` = "0.8.0-alpha")
- **Python Tools**: Dynamic version reading from VERSION file
- **CMake Integration**: Reads version from VERSION file (strips suffix for numeric version)
- **Version Flag**: `python tools/trc_analyze.py --version` shows version

### 2. Configurable Output Directory Structures

Three layout options for organizing trace files:

**Flat Layout** (default):
```cpp
trace::config.output_dir = "logs";
trace::config.output_layout = trace::Config::OutputLayout::Flat;
// Output: logs/trace_20251020_162817_821.trc
```

**ByDate Layout**:
```cpp
trace::config.output_layout = trace::Config::OutputLayout::ByDate;
// Output: logs/2025-10-20/trace_162817_821.trc
```

**BySession Layout** with auto-increment:
```cpp
trace::config.output_layout = trace::Config::OutputLayout::BySession;
trace::config.current_session = 0;  // 0 = auto-increment
// First dump: logs/session_001/trace_20251020_162817_821.trc
// Next dump:  logs/session_002/trace_20251020_162817_821.trc
```

**Features**:
- Automatic directory creation using C++17 `<filesystem>`
- Session auto-increment finds max existing session and increments
- Configurable via INI file `[dump]` section

### 3. Python Analyzer Directory Processing

Process multiple trace files at once:

```bash
# Process all .trc files in a directory
python tools/trc_analyze.py display logs/

# Recursive subdirectory search
python tools/trc_analyze.py display logs/ --recursive

# Aggregate statistics from multiple files
python tools/trc_analyze.py stats logs/ --recursive

# Sort files before processing
python tools/trc_analyze.py stats logs/ --sort-files name
```

**Sorting Options**:
- `chronological` (default): Sort by modification time
- `name`: Sort alphabetically
- `size`: Sort by file size

**Statistics Aggregation**:
- Combines events from all files for unified statistics
- Shows aggregate call counts, durations, and memory across entire directory

### 4. New Configuration Options

**C++ API**:
```cpp
trace::config.dump_suffix = ".trc";     // File extension
trace::config.output_dir = "logs";       // Output directory
trace::config.output_layout = ...;       // Layout type
trace::config.current_session = 0;       // Session number (0 = auto)
```

**INI File** (`[dump]` section):
```ini
[dump]
prefix = myapp
suffix = .trc
output_dir = logs
layout = date      # Options: flat, date, session
session = 0        # 0 = auto-increment
```

## Files Modified

**Core Library**:
- `VERSION` (new) - Single source of truth for version
- `include/trace-scope/trace_scope.hpp` - Version constants, OutputLayout enum, directory support
- `CMakeLists.txt` - Reads VERSION file
- `.gitignore` - Changed `*.bin` to `*.trc`

**Python Tools**:
- `tools/trc_common.py` - Added version reading from VERSION file
- `tools/trc_analyze.py` - Directory processing, --version flag, updated arguments

**Configuration & Examples**:
- `examples/trace_config.ini` - Updated [dump] section
- `examples/example_test_v08.cpp` (new) - Demonstrates all new features
- `examples/CMakeLists.txt` - Added example_test_v08 target

**Documentation**:
- `README.md` - Comprehensive updates for all new features
- `HISTORY.md` - Added v0.8.0-alpha entry with full details

## Testing

All features have been thoroughly tested:

✅ C++ Library:
- File extension (.trc) generation
- Flat/ByDate/BySession layouts
- Directory auto-creation
- Session auto-increment

✅ Python Tools:
- Version flag works correctly
- Directory processing (single file, flat dir, recursive)
- Statistics aggregation from multiple files
- File sorting (chronological/name/size)

✅ Integration:
- Example program generates 4 trace files in different layouts
- Python analyzer successfully processes all files
- Aggregates 177 events across 4 files correctly

## Upgrade Guide

1. **Regenerate all trace files** - old `.bin` files not supported
2. **Update scripts** that reference `.bin` files to use `.trc`
3. **Optional**: Configure output directories in your INI files
4. **Optional**: Use directory processing in your analysis scripts

## Known Issues

None - all planned features implemented and tested.

## Next Steps

Before 1.0 release:
- Consider adding backward compatibility for `.bin` files if needed
- Gather user feedback on directory layouts
- Additional testing on Linux/macOS (tested on Windows)
- Performance testing with large directories

## Credits

Developed using Cursor IDE with Claude Sonnet 4.5 AI assistance.

---

**Full details**: See `HISTORY.md` for complete implementation details and design decisions.

