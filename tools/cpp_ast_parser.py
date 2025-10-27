#!/usr/bin/env python3
"""
cpp_ast_parser.py - C++ AST parser using libclang

Parse C++ header files using clang AST for accurate extraction of:
- Namespaces
- Functions
- Structs/Classes
- Enums
- Variables
- Macros
- Includes
"""

import clang.cindex
from pathlib import Path
from typing import List, Dict, Optional, Tuple
import re

class CppASTParser:
    """Parse C++ header files using clang AST"""
    
    def __init__(self, file_path: str):
        """Initialize parser with file path"""
        self.file_path = Path(file_path)
        self.index = clang.cindex.Index.create()
        
        # Parse with preprocessor definitions
        args = ['-x', 'c++', '-std=c++17']
        self.tu = self.index.parse(str(self.file_path), args=args)
        
        # Read source content for text extraction
        self.source_content = self.file_path.read_text(encoding='utf-8')
        self.source_lines = self.source_content.splitlines(keepends=True)
    
    def extract_includes(self) -> List[str]:
        """Extract all #include directives"""
        includes = []
        for include in self.tu.get_includes():
            # In newer libclang versions, use include.include instead of include.name
            if hasattr(include, 'include'):
                includes.append(include.include.name)
            elif hasattr(include, 'name'):
                includes.append(include.name)
            else:
                # Fallback: extract from source text
                includes.extend(self._extract_includes_from_source())
        return includes
    
    def _extract_includes_from_source(self) -> List[str]:
        """Extract includes using regex as fallback"""
        includes = []
        lines = self.source_content.splitlines()
        for line in lines:
            line = line.strip()
            if line.startswith('#include'):
                # Extract the include name
                match = re.match(r'#include\s*[<"]([^>"]+)[>"]', line)
                if match:
                    includes.append(match.group(1))
        return includes
    
    def extract_namespaces(self) -> Dict[str, str]:
        """Extract namespace content by name"""
        namespaces = {}
        for cursor in self.tu.cursor.walk_preorder():
            if cursor.kind == clang.cindex.CursorKind.NAMESPACE:
                name = cursor.spelling
                content = self._extract_cursor_content(cursor)
                namespaces[name] = content
        return namespaces
    
    def extract_functions(self, namespace: str = None) -> List[Dict]:
        """Extract function definitions"""
        functions = []
        for cursor in self._find_namespace(namespace):
            if cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL:
                func_info = {
                    'name': cursor.spelling,
                    'return_type': cursor.result_type.spelling if cursor.result_type else '',
                    'content': self._extract_cursor_content(cursor),
                    'location': cursor.location,
                    'is_inline': self._is_inline_function(cursor)
                }
                functions.append(func_info)
        return functions
    
    def extract_structs(self, namespace: str = None) -> List[Dict]:
        """Extract struct/class definitions"""
        structs = []
        for cursor in self._find_namespace(namespace):
            if cursor.kind in [clang.cindex.CursorKind.STRUCT_DECL, clang.cindex.CursorKind.CLASS_DECL]:
                struct_info = {
                    'name': cursor.spelling,
                    'kind': 'struct' if cursor.kind == clang.cindex.CursorKind.STRUCT_DECL else 'class',
                    'content': self._extract_cursor_content(cursor),
                    'location': cursor.location,
                    'fields': self._extract_struct_fields(cursor)
                }
                structs.append(struct_info)
        return structs
    
    def extract_enums(self, namespace: str = None) -> List[Dict]:
        """Extract enum definitions"""
        enums = []
        for cursor in self._find_namespace(namespace):
            if cursor.kind == clang.cindex.CursorKind.ENUM_DECL:
                enum_info = {
                    'name': cursor.spelling,
                    'content': self._extract_cursor_content(cursor),
                    'location': cursor.location,
                    'values': self._extract_enum_values(cursor)
                }
                enums.append(enum_info)
        return enums
    
    def extract_variables(self, namespace: str = None) -> List[Dict]:
        """Extract variable declarations"""
        variables = []
        for cursor in self._find_namespace(namespace):
            if cursor.kind == clang.cindex.CursorKind.VAR_DECL:
                var_info = {
                    'name': cursor.spelling,
                    'type': cursor.type.spelling if cursor.type else '',
                    'content': self._extract_cursor_content(cursor),
                    'location': cursor.location,
                    'is_static': cursor.storage_class == clang.cindex.StorageClass.STATIC
                }
                variables.append(var_info)
        return variables
    
    def extract_macros(self) -> List[str]:
        """Extract macro definitions using regex (preprocessor directives)"""
        macros = []
        lines = self.source_content.splitlines()
        
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
            
            # Extract macro definitions
            if line.startswith('#define'):
                macro_lines = [line]
                j = i + 1
                
                # Handle multi-line macros
                while j < len(lines) and lines[j].rstrip().endswith('\\'):
                    macro_lines.append(lines[j])
                    j += 1
                
                macros.append('\n'.join(macro_lines))
                i = j
            else:
                i += 1
        
        return macros
    
    def _find_namespace(self, namespace: str = None):
        """Find cursors within a specific namespace"""
        if namespace is None:
            # Return all cursors at translation unit level
            return self.tu.cursor.get_children()
        
        # Find the specific namespace
        for cursor in self.tu.cursor.walk_preorder():
            if (cursor.kind == clang.cindex.CursorKind.NAMESPACE and 
                cursor.spelling == namespace):
                return cursor.get_children()
        
        return []
    
    def _extract_cursor_content(self, cursor) -> str:
        """Get the actual source text for a cursor"""
        start = cursor.extent.start
        end = cursor.extent.end
        
        if start.file and end.file and start.file.name == end.file.name:
            # Extract text from the file
            start_line = start.line - 1  # Convert to 0-based
            end_line = end.line - 1
            
            if start_line < len(self.source_lines) and end_line < len(self.source_lines):
                # Get the lines
                lines = self.source_lines[start_line:end_line + 1]
                
                # Adjust for column positions
                if start_line == end_line:
                    # Single line - extract from start.column to end.column
                    line = lines[0]
                    start_col = start.column - 1  # Convert to 0-based
                    end_col = end.column - 1
                    return line[start_col:end_col]
                else:
                    # Multiple lines
                    if lines:
                        # Adjust first line
                        lines[0] = lines[0][start.column - 1:]
                        # Adjust last line
                        lines[-1] = lines[-1][:end.column - 1]
                    return ''.join(lines)
        
        return ""
    
    def _is_inline_function(self, cursor) -> bool:
        """Check if function is inline"""
        content = self._extract_cursor_content(cursor)
        return 'inline' in content
    
    def _extract_struct_fields(self, cursor) -> List[Dict]:
        """Extract fields from struct/class"""
        fields = []
        for child in cursor.get_children():
            if child.kind == clang.cindex.CursorKind.FIELD_DECL:
                field_info = {
                    'name': child.spelling,
                    'type': child.type.spelling if child.type else '',
                    'content': self._extract_cursor_content(child)
                }
                fields.append(field_info)
        return fields
    
    def _extract_enum_values(self, cursor) -> List[str]:
        """Extract enum values"""
        values = []
        for child in cursor.get_children():
            if child.kind == clang.cindex.CursorKind.ENUM_CONSTANT_DECL:
                values.append(child.spelling)
        return values
    
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
    
    def get_diagnostics(self) -> List[str]:
        """Get parsing diagnostics"""
        diagnostics = []
        for diag in self.tu.diagnostics:
            diagnostics.append(f"{diag.severity}: {diag.spelling}")
        return diagnostics
    
    def is_valid(self) -> bool:
        """Check if the parsed file is valid C++"""
        for diag in self.tu.diagnostics:
            if diag.severity >= clang.cindex.Diagnostic.Error:
                return False
        return True

def test_parser():
    """Test the AST parser with a simple example"""
    test_content = """
    #pragma once
    #include <iostream>
    #include <string>
    
    #define TEST_MACRO(x) ((x) * 2)
    
    namespace test {
        enum class Color {
            Red,
            Green,
            Blue
        };
        
        struct Point {
            int x;
            int y;
        };
        
        inline void hello() {
            std::cout << "Hello!" << std::endl;
        }
        
        static int global_var = 42;
    }
    """
    
    # Write test file
    test_file = Path("test_parser.cpp")
    test_file.write_text(test_content)
    
    try:
        parser = CppASTParser(str(test_file))
        
        print("=== AST Parser Test ===")
        print(f"Valid: {parser.is_valid()}")
        
        print("\nIncludes:")
        for include in parser.extract_includes():
            print(f"  {include}")
        
        print("\nNamespaces:")
        namespaces = parser.extract_namespaces()
        for name, content in namespaces.items():
            print(f"  {name}: {len(content)} chars")
        
        print("\nFunctions:")
        functions = parser.extract_functions("test")
        for func in functions:
            print(f"  {func['name']} (inline: {func['is_inline']})")
        
        print("\nStructs:")
        structs = parser.extract_structs("test")
        for struct in structs:
            print(f"  {struct['name']} ({struct['kind']})")
        
        print("\nEnums:")
        enums = parser.extract_enums("test")
        for enum in enums:
            print(f"  {enum['name']}: {enum['values']}")
        
        print("\nVariables:")
        variables = parser.extract_variables("test")
        for var in variables:
            print(f"  {var['name']} ({var['type']})")
        
        print("\nMacros:")
        macros = parser.extract_macros()
        for macro in macros:
            print(f"  {macro.split()[1] if macro.split() else 'unknown'}")
        
    finally:
        # Clean up
        if test_file.exists():
            test_file.unlink()

if __name__ == "__main__":
    test_parser()
