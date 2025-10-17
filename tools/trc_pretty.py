#!/usr/bin/env python3
"""
Pretty-printer for trace_scope binary dump files.

Binary format (TRCLOG10 version 1):
  Header: "TRCLOG10" (8 bytes) + version (4) + padding (4)
  Events: type(1) + tid(4) + ts_ns(8) + depth(4) + dur_ns(8) +
          file_len(2) + file_str + func_len(2) + func_str +
          msg_len(2) + msg_str + line(4)
"""
import struct
import sys

def readn(f, n):
    """Read exactly n bytes or raise EOFError."""
    b = f.read(n)
    if len(b) < n:
        raise EOFError
    return b

def read_str(f):
    """Read length-prefixed string (2-byte length + string)."""
    (n,) = struct.unpack('<H', readn(f, 2))
    return readn(f, n).decode('utf-8', errors='replace') if n > 0 else ''

def format_duration(dur_ns):
    """Format duration with auto-scaled units matching C++ output."""
    if dur_ns < 1000:
        return f'{dur_ns} ns'
    elif dur_ns < 1000000:
        return f'{dur_ns/1000:.2f} us'
    elif dur_ns < 1000000000:
        return f'{dur_ns/1000000:.2f} ms'
    else:
        return f'{dur_ns/1000000000:.3f} s'

def main(path):
    """Parse and pretty-print a trace binary file."""
    with open(path, 'rb') as f:
        # Read header
        magic = readn(f, 8)
        if magic != b'TRCLOG10':
            raise SystemExit(f'Error: Bad magic header (expected TRCLOG10, got {magic})')
        
        version, padding = struct.unpack('<II', readn(f, 8))
        if version != 1:
            raise SystemExit(f'Error: Unsupported version {version} (expected 1)')
        
        # Read events until EOF
        event_count = 0
        try:
            while True:
                # Read event (matches C++ dump_binary format)
                (typ,) = struct.unpack('<B', readn(f, 1))
                (tid,) = struct.unpack('<I', readn(f, 4))
                (ts_ns,) = struct.unpack('<Q', readn(f, 8))
                (depth,) = struct.unpack('<I', readn(f, 4))
                (dur_ns,) = struct.unpack('<Q', readn(f, 8))
                
                # Read length-prefixed strings
                file = read_str(f)
                func = read_str(f)
                msg = read_str(f)
                
                (line,) = struct.unpack('<I', readn(f, 4))
                
                # Format and print event
                ts = f'[{ts_ns}] '
                th = f'({tid:08x}) '
                ind = '| ' * max(depth, 0)  # Visual indent markers
                
                if typ == 0:  # Enter
                    print(f'{ts}{th}{ind}-> {func} ({file}:{line})')
                elif typ == 1:  # Exit
                    print(f'{ts}{th}{ind}<- {func}  [{format_duration(dur_ns)}] ({file}:{line})')
                else:  # Msg
                    print(f'{ts}{th}{ind}- {msg} ({file}:{line})')
                
                event_count += 1
                
        except EOFError:
            # Normal end of file
            pass
    
    print(f'\n# Processed {event_count} events', file=sys.stderr)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: trc_pretty.py trace.bin', file=sys.stderr)
        sys.exit(2)
    main(sys.argv[1])
