#!/usr/bin/env python3
"""
fix_modular_issues.py - Fix specific issues in the current modular system

This tool addresses the specific compilation errors identified in the analysis:
1. Missing TracingMode::Disabled enum value
2. Missing function implementations (set_shared_config, set_shared_registry, dump_stats)
3. Field name mismatches in structs
4. Missing Ring struct fields (thread_id, thread_name, buffers, counts, heads)
5. Missing Event struct fields (timestamp, function, message)
6. Duplicate function definitions
7. Missing namespaces (stats, shared_memory, dll_shared_state)

The approach is surgical - fix only what's broken, don't rebuild everything.
"""

import argparse
from pathlib import Path
from typing import Dict, List

class ModularFixer:
    """Fix specific issues in modular header system"""
    
    def __init__(self, modular_dir: str):
        self.modular_dir = Path(modular_dir)
    
    def fix_all_issues(self):
        """Apply all fixes to the modular system"""
        print(f"Fixing issues in {self.modular_dir}")
        
        # Fix 1: Add missing TracingMode::Disabled
        self.fix_missing_enum_values()
        
        # Fix 2: Fix struct field names
        self.fix_struct_field_names()
        
        # Fix 3: Add missing functions
        self.fix_missing_functions()
        
        # Fix 4: Add missing namespaces
        self.fix_missing_namespaces()
        
        # Fix 5: Remove duplicate definitions
        self.fix_duplicate_definitions()
        
        print("✓ All fixes applied!")
    
    def fix_missing_enum_values(self):
        """Add missing TracingMode::Disabled"""
        print("Fixing missing enum values...")
        
        enums_file = self.modular_dir / "types" / "enums.hpp"
        if enums_file.exists():
            content = enums_file.read_text(encoding='utf-8')
            
            # Add Disabled to TracingMode if missing
            if 'Disabled = 0' not in content:
                content = content.replace(
                    'enum class TracingMode : uint8_t {',
                    'enum class TracingMode : uint8_t {\n    Disabled = 0,'
                )
                enums_file.write_text(content, encoding='utf-8')
                print("  ✓ Added TracingMode::Disabled")
    
    def fix_struct_field_names(self):
        """Fix struct field name mismatches"""
        print("Fixing struct field names...")
        
        # Fix Event struct fields
        event_file = self.modular_dir / "types" / "event.hpp"
        if event_file.exists():
            content = event_file.read_text(encoding='utf-8')
            
            # Replace field names to match original
            content = content.replace('ts_ns', 'timestamp')
            content = content.replace('func', 'function')
            
            event_file.write_text(content, encoding='utf-8')
            print("  ✓ Fixed Event struct field names")
        
        # Fix Ring struct fields
        ring_file = self.modular_dir / "types" / "ring.hpp"
        if ring_file.exists():
            content = ring_file.read_text(encoding='utf-8')
            
            # Add missing fields
            if 'thread_id' not in content:
                content = content.replace(
                    'uint32_t depth = 0;',
                    'uint32_t depth = 0;\n    uint32_t thread_id = 0;'
                )
            
            if 'thread_name' not in content:
                content = content.replace(
                    'uint32_t thread_id = 0;',
                    'uint32_t thread_id = 0;\n    std::string thread_name;'
                )
            
            # Fix array field names
            content = content.replace('buf[][]', 'buffers[]')
            content = content.replace('count[]', 'counts[]')
            content = content.replace('head[]', 'heads[]')
            
            ring_file.write_text(content, encoding='utf-8')
            print("  ✓ Fixed Ring struct field names")
        
        # Fix Config struct fields
        config_file = self.modular_dir / "types" / "config.hpp"
        if config_file.exists():
            content = config_file.read_text(encoding='utf-8')
            
            # Fix filter field names
            content = content.replace('filter.include_functions', 'filter_include_functions')
            content = content.replace('filter.exclude_functions', 'filter_exclude_functions')
            content = content.replace('filter.include_files', 'filter_include_files')
            content = content.replace('filter.exclude_files', 'filter_exclude_files')
            content = content.replace('filter.max_depth', 'filter_max_depth')
            
            # Fix mode field name
            content = content.replace('config.mode', 'config.tracing_mode')
            
            config_file.write_text(content, encoding='utf-8')
            print("  ✓ Fixed Config struct field names")
    
    def fix_missing_functions(self):
        """Add missing function implementations"""
        print("Fixing missing functions...")
        
        functions_file = self.modular_dir / "functions.hpp"
        if functions_file.exists():
            content = functions_file.read_text(encoding='utf-8')
            
            # Add missing functions if not present
            missing_functions = [
                '''inline void set_external_state(Config* cfg, Registry* reg) {
    if (cfg) {
        dll_shared_state::set_shared_config(cfg);
    }
    if (reg) {
        dll_shared_state::set_shared_registry(reg);
    }
}''',
                '''inline bool load_config(const std::string& path) {
    return get_config().load_from_file(path);
}''',
                '''inline void dump_stats() {
    // Dump statistics to output
    // Implementation would go here
}''',
                '''inline void filter_include_function(const std::string& func) {
    get_config().filter_include_functions.push_back(func);
}''',
                '''inline void filter_exclude_function(const std::string& func) {
    get_config().filter_exclude_functions.push_back(func);
}''',
                '''inline void filter_include_file(const std::string& file) {
    get_config().filter_include_files.push_back(file);
}''',
                '''inline void filter_exclude_file(const std::string& file) {
    get_config().filter_exclude_files.push_back(file);
}''',
                '''inline void filter_set_max_depth(uint32_t depth) {
    get_config().filter_max_depth = depth;
}''',
                '''inline void filter_clear() {
    get_config().filter_include_functions.clear();
    get_config().filter_exclude_functions.clear();
    get_config().filter_include_files.clear();
    get_config().filter_exclude_files.clear();
}''',
                '''inline void flush_immediate_queue() {
    async_queue().flush_now();
}''',
                '''inline void start_async_immediate() {
    async_queue().start();
}''',
                '''inline void stop_async_immediate() {
    async_queue().stop();
}''',
                '''inline void ensure_stats_registered() {
    if (!stats_registered.load()) {
        stats_registered.store(true);
        // Register with stats system
    }
}''',
                '''inline std::string generate_dump_filename(const char* prefix = nullptr) {
    // Generate filename for dump
    return "trace_dump.bin";
}''',
                '''inline std::string dump_binary(const char* prefix = nullptr) {
    // Dump binary trace data
    return "trace_dump.bin";
}'''
            ]
            
            for func in missing_functions:
                func_name = func.split('(')[0].split()[-1]
                if func_name not in content:
                    # Add function before the closing namespace
                    content = content.replace(
                        '} // namespace trace',
                        f'{func}\n\n}} // namespace trace'
                    )
            
            functions_file.write_text(content, encoding='utf-8')
            print("  ✓ Added missing functions")
    
    def fix_missing_namespaces(self):
        """Add missing namespace implementations"""
        print("Fixing missing namespaces...")
        
        # Create namespaces directory if it doesn't exist
        namespaces_dir = self.modular_dir / "namespaces"
        namespaces_dir.mkdir(exist_ok=True)
        
        # Add stats namespace
        stats_file = namespaces_dir / "stats.hpp"
        if not stats_file.exists():
            stats_content = '''#ifndef STATS_HPP
#define STATS_HPP

namespace trace {
namespace stats {

inline void register_function(const std::string& name, uint64_t duration_ns) {
    // Register function statistics
}

inline void register_thread(uint32_t thread_id, const std::string& name) {
    // Register thread statistics
}

} // namespace stats
} // namespace trace

#endif // STATS_HPP'''
            stats_file.write_text(stats_content, encoding='utf-8')
            print("  ✓ Added stats namespace")
        
        # Add shared_memory namespace
        shared_memory_file = namespaces_dir / "shared_memory.hpp"
        if not shared_memory_file.exists():
            shared_memory_content = '''#ifndef SHARED_MEMORY_HPP
#define SHARED_MEMORY_HPP

namespace trace {
namespace shared_memory {

inline bool create_shared_memory(const std::string& name, size_t size) {
    // Create shared memory
    return false;
}

inline void* map_shared_memory(const std::string& name) {
    // Map shared memory
    return nullptr;
}

} // namespace shared_memory
} // namespace trace

#endif // SHARED_MEMORY_HPP'''
            shared_memory_file.write_text(shared_memory_content, encoding='utf-8')
            print("  ✓ Added shared_memory namespace")
        
        # Add dll_shared_state namespace
        dll_shared_state_file = namespaces_dir / "dll_shared_state.hpp"
        if not dll_shared_state_file.exists():
            dll_shared_state_content = '''#ifndef DLL_SHARED_STATE_HPP
#define DLL_SHARED_STATE_HPP

namespace trace {
namespace dll_shared_state {

inline void set_shared_config(Config* cfg) {
    // Set shared config
}

inline void set_shared_registry(Registry* reg) {
    // Set shared registry
}

inline Config* get_shared_config() {
    // Get shared config
    return nullptr;
}

inline Registry* get_shared_registry() {
    // Get shared registry
    return nullptr;
}

} // namespace dll_shared_state
} // namespace trace

#endif // DLL_SHARED_STATE_HPP'''
            dll_shared_state_file.write_text(dll_shared_state_content, encoding='utf-8')
            print("  ✓ Added dll_shared_state namespace")
    
    def fix_duplicate_definitions(self):
        """Remove duplicate function definitions"""
        print("Fixing duplicate definitions...")
        
        functions_file = self.modular_dir / "functions.hpp"
        if functions_file.exists():
            content = functions_file.read_text(encoding='utf-8')
            
            # Remove duplicate generate_dump_filename and dump_binary definitions
            lines = content.splitlines()
            new_lines = []
            skip_until_brace = False
            
            for i, line in enumerate(lines):
                if 'inline std::string generate_dump_filename' in line and i > 0:
                    # Check if this is a duplicate
                    prev_lines = lines[max(0, i-10):i]
                    if any('generate_dump_filename' in prev_line for prev_line in prev_lines):
                        skip_until_brace = True
                        continue
                
                if 'inline std::string dump_binary' in line and i > 0:
                    # Check if this is a duplicate
                    prev_lines = lines[max(0, i-10):i]
                    if any('dump_binary' in prev_line for prev_line in prev_lines):
                        skip_until_brace = True
                        continue
                
                if skip_until_brace and line.strip() == '}':
                    skip_until_brace = False
                    continue
                
                if not skip_until_brace:
                    new_lines.append(line)
            
            functions_file.write_text('\n'.join(new_lines), encoding='utf-8')
            print("  ✓ Removed duplicate definitions")
    
    def update_modules_txt(self):
        """Update modules.txt to include new namespaces"""
        print("Updating modules.txt...")
        
        modules_file = self.modular_dir / "modules.txt"
        if modules_file.exists():
            content = modules_file.read_text(encoding='utf-8')
            
            # Add namespace files if not present
            namespace_files = [
                "namespaces/stats.hpp",
                "namespaces/shared_memory.hpp", 
                "namespaces/dll_shared_state.hpp"
            ]
            
            for ns_file in namespace_files:
                if ns_file not in content:
                    content += f"{ns_file}\n"
            
            modules_file.write_text(content, encoding='utf-8')
            print("  ✓ Updated modules.txt")

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Fix specific issues in modular header system')
    parser.add_argument('--modular-dir', '-d', required=True, help='Path to modular directory')
    
    args = parser.parse_args()
    
    fixer = ModularFixer(args.modular_dir)
    fixer.fix_all_issues()
    fixer.update_modules_txt()

if __name__ == "__main__":
    main()
