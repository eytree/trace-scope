#!/usr/bin/env python3
import struct, sys

def readn(f, n):
    b = f.read(n)
    if len(b) < n: raise EOFError
    return b

def read_str(f, n):
    return readn(f, n).decode('utf-8', errors='replace') if n>0 else ''

def main(path):
    with open(path, 'rb') as f:
        if readn(f,8) != b'TRCLOG10': raise SystemExit('bad magic')
        version, _ = struct.unpack('<II', readn(f,8))
        if version != 1: raise SystemExit('bad version')
        while True:
            hdr = f.read(4)
            if not hdr: break
            (count,) = struct.unpack('<I', hdr)
            for _ in range(count):
                ts_ns, tid = struct.unpack('<QI', readn(f,12))
                (typ,)    = struct.unpack('<B', readn(f,1))
                (depth,)  = struct.unpack('<i', readn(f,4))
                (dur_ns,) = struct.unpack('<Q', readn(f,8))
                (line,)   = struct.unpack('<I', readn(f,4))
                func_len, file_len, msg_len = struct.unpack('<HHH', readn(f,6))
                func = read_str(f, func_len)
                file = read_str(f, file_len)
                msg  = read_str(f, msg_len)

                ts = f'[{ts_ns}] '
                th = f'(tid={tid:08x}) '
                ind = '  ' * max(depth,0)
                if typ==0:
                    print(f'{ts}{th}{ind}↘ enter {func} ({file}:{line})')
                elif typ==1:
                    dur_us = dur_ns//1000
                    print(f'{ts}{th}{ind}↖ exit  {func}  [{dur_us} µs] ({file}:{line})')
                else:
                    print(f'{ts}{th}{ind}• {msg}  ({file}:{line})')

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('usage: trc_pretty.py trace.bin', file=sys.stderr)
        sys.exit(2)
    main(sys.argv[1])
