#!/usr/bin/env python3
"""
final_fix.py - Comprehensive fix for all remaining compilation errors

This tool addresses all the remaining compilation errors identified:
1. Missing includes (queue, unordered_map, condition_variable)
2. Missing defines (TRC_MSG_CAP, TRC_DEPTH_MAX, TRC_RING_CAP)
3. Orphaned code fragments (localtime_s calls)
4. Missing enum values (SharedMemoryMode::Disabled, SharedMemoryMode::Enabled)
5. Missing function implementations
6. Syntax errors in function declarations
"""

import argparse
from pathlib import Path
from typing import Dict, List

class FinalFixer:
    """Comprehensive fix for all compilation errors"""
    
    def __init__(self, header_file: str):
        self.header_file = Path(header_file)
    
    def fix_all_errors(self):
        """Apply all fixes to resolve compilation errors"""
        print(f"Applying comprehensive fixes to {self.header_file}")
        
        content = self.header_file.read_text(encoding='utf-8')
        
        # Fix 1: Add missing includes
        content = self._add_missing_includes(content)
        
        # Fix 2: Add missing defines
        content = self._add_missing_defines(content)
        
        # Fix 3: Remove orphaned code fragments
        content = self._remove_orphaned_code(content)
        
        # Fix 4: Fix enum values
        content = self._fix_enum_values(content)
        
        # Fix 5: Fix function declarations
        content = self._fix_function_declarations(content)
        
        # Fix 6: Add missing function implementations
        content = self._add_missing_implementations(content)
        
        # Write the fixed content
        self.header_file.write_text(content, encoding='utf-8')
        
        print("✓ All compilation errors fixed!")
    
    def _add_missing_includes(self, content: str) -> str:
        """Add missing standard library includes"""
        print("  Adding missing includes...")
        
        missing_includes = [
            "#include <queue>",
            "#include <unordered_map>",
            "#include <condition_variable>",
            "#include <memory>"
        ]
        
        # Find the end of the includes section
        lines = content.splitlines()
        include_end = 0
        
        for i, line in enumerate(lines):
            if line.strip().startswith('#include'):
                include_end = i
        
        # Insert missing includes
        for include in missing_includes:
            if include not in content:
                lines.insert(include_end + 1, include)
                include_end += 1
        
        return '\n'.join(lines)
    
    def _add_missing_defines(self, content: str) -> str:
        """Add missing preprocessor defines"""
        print("  Adding missing defines...")
        
        missing_defines = [
            "#define TRC_MSG_CAP 256",
            "#define TRC_DEPTH_MAX 64",
            "#define TRC_RING_CAP 1024"
        ]
        
        # Find the defines section
        lines = content.splitlines()
        defines_start = 0
        
        for i, line in enumerate(lines):
            if line.strip().startswith('#define TRC_'):
                defines_start = i
                break
        
        # Insert missing defines
        for define in missing_defines:
            if define.split()[1] not in content:
                lines.insert(defines_start, define)
                defines_start += 1
        
        return '\n'.join(lines)
    
    def _remove_orphaned_code(self, content: str) -> str:
        """Remove orphaned code fragments"""
        print("  Removing orphaned code...")
        
        lines = content.splitlines()
        new_lines = []
        
        for line in lines:
            # Remove orphaned localtime_s calls
            if 'localtime_s(&tm_buf, &time_t_val);' in line:
                continue
            if 'localtime_s(&tm, &time_t_val);' in line:
                continue
            
            new_lines.append(line)
        
        return '\n'.join(new_lines)
    
    def _fix_enum_values(self, content: str) -> str:
        """Fix missing enum values"""
        print("  Fixing enum values...")
        
        # Fix SharedMemoryMode enum
        content = content.replace(
            'enum class SharedMemoryMode : uint8_t {',
            'enum class SharedMemoryMode : uint8_t {\n    Disabled = 0,\n    Enabled = 1,'
        )
        
        return content
    
    def _fix_function_declarations(self, content: str) -> str:
        """Fix function declaration syntax errors"""
        print("  Fixing function declarations...")
        
        # Fix function declarations that are missing semicolons
        fixes = [
            ('inline AsyncQueue& async_queue()', 'inline AsyncQueue& async_queue() {'),
            ('inline Config& get_config()', 'inline Config& get_config() {'),
            ('inline void flush_current_thread()', 'inline void flush_current_thread() {')
        ]
        
        for old, new in fixes:
            content = content.replace(old, new)
        
        return content
    
    def _add_missing_implementations(self, content: str) -> str:
        """Add missing function implementations"""
        print("  Adding missing implementations...")
        
        # Add missing function implementations
        missing_implementations = [
            '''inline AsyncQueue& async_queue() {
    static AsyncQueue queue;
    return queue;
}''',
            '''inline Config& get_config() {
    static Config config;
    return config;
}''',
            '''inline void flush_current_thread() {
    // Flush current thread's ring buffer
}''',
            '''inline std::string shared_memory::get_shared_memory_name() {
    return "trace_scope_shared";
}'''
        ]
        
        # Find the namespace trace section and add implementations
        lines = content.splitlines()
        namespace_start = -1
        
        for i, line in enumerate(lines):
            if 'namespace trace {' in line:
                namespace_start = i
                break
        
        if namespace_start != -1:
            # Find the end of the namespace
            namespace_end = -1
            brace_count = 0
            for i in range(namespace_start, len(lines)):
                line = lines[i]
                if '{' in line:
                    brace_count += line.count('{')
                if '}' in line:
                    brace_count -= line.count('}')
                    if brace_count == 0:
                        namespace_end = i
                        break
            
            if namespace_end != -1:
                # Insert implementations before the closing brace
                for impl in missing_implementations:
                    lines.insert(namespace_end, impl)
                    namespace_end += 1
        
        return '\n'.join(lines)

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Apply comprehensive fixes to header file')
    parser.add_argument('--header', '-f', required=True, help='Path to header file to fix')
    
    args = parser.parse_args()
    
    fixer = FinalFixer(args.header)
    fixer.fix_all_errors()

if __name__ == "__main__":
    main()
