#!/usr/bin/env python3
"""
extract_modular.py - Extract modular headers from monolithic header using AST parser

Extract modular headers from trace_scope_original.hpp using libclang AST parsing
for accurate extraction of namespaces, functions, structs, enums, etc.
"""

import argparse
from pathlib import Path
from typing import Dict, List
from cpp_ast_parser import CppASTParser

class ModularExtractor:
    """Extract modular headers from monolithic header"""
    
    def __init__(self, source_file: str, output_dir: str):
        self.source_file = Path(source_file)
        self.output_dir = Path(output_dir)
        self.parser = CppASTParser(str(self.source_file))
        
        # Create output directory structure
        self.output_dir.mkdir(parents=True, exist_ok=True)
        (self.output_dir / "types").mkdir(exist_ok=True)
        (self.output_dir / "namespaces").mkdir(exist_ok=True)
    
    def extract_all(self):
        """Extract all components to modular structure"""
        print(f"Extracting from {self.source_file} to {self.output_dir}")
        
        # Extract platform-specific content
        self.extract_platform()
        
        # Extract types
        self.extract_enums()
        self.extract_structs()
        
        # Extract functions and variables
        self.extract_functions()
        self.extract_variables()
        
        # Extract namespaces
        self.extract_namespaces()
        
        # Extract macros
        self.extract_macros()
        
        # Generate modules.txt
        self.generate_modules_txt()
        
        print("✓ Extraction complete!")
    
    def extract_platform(self):
        """Extract platform includes and defines"""
        print("Extracting platform content...")
        
        # Extract includes
        includes = self.parser.extract_includes()
        
        # Extract defines from source (preprocessor directives)
        defines = self._extract_defines_from_source()
        
        content = self._generate_header(
            "platform.hpp",
            "Platform-specific includes and build-time defines",
            includes=includes,
            defines=defines
        )
        
        self._write_file("platform.hpp", content)
    
    def extract_enums(self):
        """Extract enums to types/enums.hpp"""
        print("Extracting enums...")
        
        enums = self.parser.extract_enums(namespace="trace")
        
        if enums:
            content = self._generate_header(
                "enums.hpp",
                "Enum definitions",
                enums=enums
            )
            self._write_file("types/enums.hpp", content)
        else:
            print("  No enums found in trace namespace")
    
    def extract_structs(self):
        """Extract each struct to its own file"""
        print("Extracting structs...")
        
        structs = self.parser.extract_structs(namespace="trace")
        
        for struct in structs:
            filename = f"{struct['name'].lower()}.hpp"
            content = self._generate_header(
                f"{struct['name']}.hpp",
                f"{struct['name']} struct definition",
                structs=[struct]
            )
            self._write_file(f"types/{filename}", content)
            print(f"  Extracted {struct['name']} -> types/{filename}")
    
    def extract_functions(self):
        """Extract functions to functions.hpp"""
        print("Extracting functions...")
        
        functions = self.parser.extract_functions(namespace="trace")
        
        if functions:
            content = self._generate_header(
                "functions.hpp",
                "Function implementations",
                functions=functions
            )
            self._write_file("functions.hpp", content)
            print(f"  Extracted {len(functions)} functions")
        else:
            print("  No functions found in trace namespace")
    
    def extract_variables(self):
        """Extract variables to variables.hpp"""
        print("Extracting variables...")
        
        variables = self.parser.extract_variables(namespace="trace")
        
        if variables:
            content = self._generate_header(
                "variables.hpp",
                "Global variable declarations",
                variables=variables
            )
            self._write_file("variables.hpp", content)
            print(f"  Extracted {len(variables)} variables")
        else:
            print("  No variables found in trace namespace")
    
    def extract_namespaces(self):
        """Extract nested namespaces to separate files"""
        print("Extracting namespaces...")
        
        namespaces = self.parser.extract_namespaces()
        
        for name, content in namespaces.items():
            if name == "trace":
                continue  # Skip main namespace
            
            filename = f"{name}.hpp"
            header_content = self._generate_header(
                f"{name}.hpp",
                f"namespace {name}",
                namespace_content=content
            )
            self._write_file(f"namespaces/{filename}", header_content)
            print(f"  Extracted namespace {name} -> namespaces/{filename}")
    
    def extract_macros(self):
        """Extract macros to macros.hpp"""
        print("Extracting macros...")
        
        macros = self.parser.extract_macros()
        
        if macros:
            content = self._generate_header(
                "macros.hpp",
                "TRC_* macro definitions",
                macros=macros
            )
            self._write_file("macros.hpp", content)
            print(f"  Extracted {len(macros)} macros")
        else:
            print("  No macros found")
    
    def generate_modules_txt(self):
        """Generate modules.txt with merge order"""
        print("Generating modules.txt...")
        
        modules = [
            "platform.hpp",
            "types/enums.hpp",
            "types/event.hpp",
            "types/stats.hpp",
            "types/config.hpp",
            "types/async_queue.hpp",
            "types/ring.hpp",
            "types/registry.hpp",
            "types/scope.hpp",
            "variables.hpp",
            "functions.hpp",
            "namespaces/stats.hpp",
            "namespaces/shared_memory.hpp",
            "namespaces/dll_shared_state.hpp",
            "namespaces/internal.hpp",
            "macros.hpp"
        ]
        
        content = "# Merge order for trace_scope.hpp generation\n"
        content += "# Lines starting with # are comments\n"
        content += "# Order matters - dependencies must come before dependents\n\n"
        
        for module in modules:
            module_path = self.output_dir / module
            if module_path.exists():
                content += f"{module}\n"
        
        self._write_file("modules.txt", content)
    
    def _extract_defines_from_source(self) -> List[str]:
        """Extract #define directives from source"""
        defines = []
        lines = self.parser.source_content.splitlines()
        
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
    
    def _generate_header(self, filename: str, description: str, **kwargs) -> str:
        """Generate header file content"""
        guard_name = filename.upper().replace('.', '_').replace('/', '_')
        
        content = f"#ifndef {guard_name}\n"
        content += f"#define {guard_name}\n\n"
        content += f"/**\n"
        content += f" * @file {filename}\n"
        content += f" * @brief {description}\n"
        content += f" * \n"
        content += f" * Generated from trace_scope_original.hpp using AST extraction\n"
        content += f" */\n\n"
        
        # Add includes if needed
        if 'includes' in kwargs:
            for include in kwargs['includes']:
                if include.startswith('<') and include.endswith('>'):
                    content += f"#include {include}\n"
                else:
                    content += f"#include <{include}>\n"
            content += "\n"
        
        # Add defines
        if 'defines' in kwargs:
            for define in kwargs['defines']:
                content += f"{define}\n"
            content += "\n"
        
        # Add namespace content
        if 'namespace_content' in kwargs:
            content += "namespace trace {\n\n"
            content += kwargs['namespace_content']
            content += "\n} // namespace trace\n\n"
        
        # Add enums
        if 'enums' in kwargs:
            content += "namespace trace {\n\n"
            for enum in kwargs['enums']:
                content += enum['content'] + "\n\n"
            content += "} // namespace trace\n\n"
        
        # Add structs
        if 'structs' in kwargs:
            content += "namespace trace {\n\n"
            for struct in kwargs['structs']:
                content += struct['content'] + "\n\n"
            content += "} // namespace trace\n\n"
        
        # Add functions
        if 'functions' in kwargs:
            content += "namespace trace {\n\n"
            for func in kwargs['functions']:
                content += func['content'] + "\n\n"
            content += "} // namespace trace\n\n"
        
        # Add variables
        if 'variables' in kwargs:
            content += "namespace trace {\n\n"
            for var in kwargs['variables']:
                content += var['content'] + "\n\n"
            content += "} // namespace trace\n\n"
        
        # Add macros
        if 'macros' in kwargs:
            for macro in kwargs['macros']:
                content += macro + "\n"
        
        content += f"\n#endif // {guard_name}\n"
        
        return content
    
    def _write_file(self, relative_path: str, content: str):
        """Write content to file"""
        file_path = self.output_dir / relative_path
        file_path.parent.mkdir(parents=True, exist_ok=True)
        file_path.write_text(content, encoding='utf-8')

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Extract modular headers from monolithic header')
    parser.add_argument('--input', '-i', required=True, help='Input monolithic header file')
    parser.add_argument('--output', '-o', required=True, help='Output directory for modular files')
    
    args = parser.parse_args()
    
    extractor = ModularExtractor(args.input, args.output)
    extractor.extract_all()

if __name__ == "__main__":
    main()
