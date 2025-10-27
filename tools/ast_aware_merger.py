#!/usr/bin/env python3
"""
ast_aware_merger.py - Merge modular headers using AST validation

Enhanced merge tool that uses libclang AST parsing to validate modules
and ensure proper merging without the issues of regex-based parsing.
"""

import argparse
from pathlib import Path
from typing import List, Dict
from cpp_ast_parser import CppASTParser
from datetime import datetime

class ASTAwareMerger:
    """Merge modular headers using AST validation"""
    
    def __init__(self, modules_dir: str, output_file: str):
        self.modules_dir = Path(modules_dir)
        self.output_file = Path(output_file)
        self.modules = []
    
    def merge(self):
        """Merge with AST validation"""
        print(f"Merging modules from {self.modules_dir} to {self.output_file}")
        
        # Load modules in dependency order
        self._load_modules()
        
        # Validate each module is well-formed
        self._validate_modules()
        
        # Merge in correct order
        merged_content = self._merge_modules()
        
        # Validate merged result
        self._validate_merged(merged_content)
        
        # Write output
        self._write_output(merged_content)
        
        print("✓ Merge complete!")
    
    def _load_modules(self):
        """Load modules in dependency order from modules.txt"""
        modules_file = self.modules_dir / "modules.txt"
        
        if not modules_file.exists():
            raise FileNotFoundError(f"modules.txt not found in {self.modules_dir}")
        
        # Read module order
        with open(modules_file, 'r') as f:
            lines = f.readlines()
        
        for line in lines:
            line = line.strip()
            if line and not line.startswith('#'):
                module_path = self.modules_dir / line
                if module_path.exists():
                    self.modules.append(module_path)
                else:
                    print(f"Warning: Module {line} not found, skipping")
    
    def _validate_modules(self):
        """Parse each module to ensure it's valid C++"""
        print("Validating modules...")
        
        for module_path in self.modules:
            try:
                parser = CppASTParser(str(module_path))
                
                if not parser.is_valid():
                    diagnostics = parser.get_diagnostics()
                    print(f"Warning: {module_path} has diagnostics:")
                    for diag in diagnostics:
                        print(f"  {diag}")
                else:
                    print(f"✓ {module_path.name} is valid")
                    
            except Exception as e:
                print(f"Error validating {module_path}: {e}")
                # Continue with merge - some modules might have minor issues
    
    def _merge_modules(self) -> str:
        """Merge modules in dependency order"""
        print("Merging modules...")
        
        # Start with header comment
        content = self._generate_header_comment()
        
        # Extract and consolidate includes
        all_includes = set()
        all_defines = []
        namespace_content = []
        macros = []
        
        for module_path in self.modules:
            try:
                parser = CppASTParser(str(module_path))
                
                # Extract includes
                includes = parser.extract_includes()
                for include in includes:
                    all_includes.add(include)
                
                # Extract defines from source
                defines = self._extract_defines_from_file(module_path)
                all_defines.extend(defines)
                
                # Extract namespace content
                namespaces = parser.extract_namespaces()
                if 'trace' in namespaces:
                    namespace_content.append(namespaces['trace'])
                
                # Extract macros
                module_macros = parser.extract_macros()
                macros.extend(module_macros)
                
            except Exception as e:
                print(f"Warning: Error processing {module_path}: {e}")
                # Fallback to text-based extraction
                content_text = module_path.read_text(encoding='utf-8')
                namespace_content.append(content_text)
        
        # Add includes
        content += "#pragma once\n\n"
        for include in sorted(all_includes):
            if include.startswith('<') and include.endswith('>'):
                content += f"#include {include}\n"
            else:
                content += f"#include <{include}>\n"
        content += "\n"
        
        # Add defines
        for define in all_defines:
            content += f"{define}\n"
        content += "\n"
        
        # Add single namespace
        content += "// Single namespace\n"
        content += "namespace trace {\n\n"
        
        for ns_content in namespace_content:
            # Strip namespace declarations from content
            cleaned_content = self._strip_namespace_declarations(ns_content)
            content += cleaned_content + "\n"
        
        content += "} // namespace trace\n\n"
        
        # Add macros
        for macro in macros:
            content += f"{macro}\n"
        
        return content
    
    def _extract_defines_from_file(self, file_path: Path) -> List[str]:
        """Extract #define directives from file"""
        defines = []
        content = file_path.read_text(encoding='utf-8')
        lines = content.splitlines()
        
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            
            # Skip header guards
            if line.startswith('#ifndef') and i + 1 < len(lines):
                next_line = lines[i + 1].strip()
                if next_line.startswith('#define'):
                    i = self._skip_to_endif(lines, i)
                    continue
            
            # Skip #pragma once
            if line == '#pragma once':
                i += 1
                continue
            
            # Extract #define directives
            if line.startswith('#define'):
                defines.append(line)
            
            i += 1
        
        return defines
    
    def _skip_to_endif(self, lines: List[str], start_idx: int) -> int:
        """Skip to matching #endif"""
        depth = 1
        i = start_idx + 1
        
        while i < len(lines) and depth > 0:
            line = lines[i].strip()
            if line.startswith('#if'):
                depth += 1
            elif line.startswith('#endif'):
                depth -= 1
            i += 1
        
        return i
    
    def _strip_namespace_declarations(self, content: str) -> str:
        """Remove namespace trace { and } declarations from content"""
        lines = content.splitlines()
        result_lines = []
        
        i = 0
        while i < len(lines):
            line = lines[i].strip()
            
            # Skip namespace declarations
            if line.startswith('namespace trace') and '{' in line:
                i += 1
                continue
            elif line == 'namespace trace {' or line == 'namespace trace{':
                i += 1
                continue
            elif line == '} // namespace trace' or line == '}':
                i += 1
                continue
            
            result_lines.append(lines[i])
            i += 1
        
        return '\n'.join(result_lines)
    
    def _validate_merged(self, content: str):
        """Validate merged result by parsing it"""
        print("Validating merged result...")
        
        # Write to temporary file for validation
        temp_file = Path("temp_merged.hpp")
        try:
            temp_file.write_text(content, encoding='utf-8')
            
            parser = CppASTParser(str(temp_file))
            
            if parser.is_valid():
                print("✓ Merged header is valid C++")
            else:
                diagnostics = parser.get_diagnostics()
                print("Warning: Merged header has diagnostics:")
                for diag in diagnostics:
                    print(f"  {diag}")
                    
        finally:
            if temp_file.exists():
                temp_file.unlink()
    
    def _write_output(self, content: str):
        """Write merged content to output file"""
        self.output_file.parent.mkdir(parents=True, exist_ok=True)
        self.output_file.write_text(content, encoding='utf-8')
        
        # Print statistics
        lines = len(content.splitlines())
        size = len(content.encode('utf-8'))
        print(f"Generated {self.output_file}")
        print(f"  Size: {size:,} bytes")
        print(f"  Lines: {lines:,}")
    
    def _generate_header_comment(self) -> str:
        """Generate header comment"""
        timestamp = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S UTC")
        module_count = len(self.modules)
        
        comment = f"""/**
 * @file trace_scope.hpp
 * @brief Single-header C++ tracing library with per-thread ring buffers
 * 
 * DO NOT EDIT THIS FILE DIRECTLY - edit the source modules instead.
 * 
 * Generated: {timestamp}
 * Source modules: {module_count} files
 * 
 * This file is generated from modular source files using AST-based extraction
 * and merging. The modular architecture allows for better maintainability
 * while preserving the single-header library benefits.
 * 
 * For development, see:
 * - Source: include/trace-scope/trace_scope_modular/
 * - Extraction: tools/extract_modular.py
 * - Merging: tools/ast_aware_merger.py
 */

"""
        return comment

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Merge modular headers using AST validation')
    parser.add_argument('--input', '-i', required=True, help='Input directory containing modular files')
    parser.add_argument('--output', '-o', required=True, help='Output path for merged header file')
    
    args = parser.parse_args()
    
    merger = ASTAwareMerger(args.input, args.output)
    merger.merge()

if __name__ == "__main__":
    main()
