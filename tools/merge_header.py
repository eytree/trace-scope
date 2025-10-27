#!/usr/bin/env python3
"""
merge_header.py - Enhanced C++ header merger

Merges multiple C++ header files into a single properly structured header, handling:
- Header guards (#ifndef/#define/#endif)
- #pragma once
- Include directives (consolidated at top)
- Preprocessor defines (after includes)
- Namespace consolidation (single namespace trace block)
- Macro dependency ordering
- Comment preservation

Usage:
    python merge_header.py --input include/trace-scope/trace_scope_modular --output include/trace-scope/trace_scope.hpp
"""

import argparse
import re
from datetime import datetime
from pathlib import Path
from typing import List, Set, Dict, Tuple
from collections import defaultdict

class HeaderParser:
    """Parse C++ header files and extract structured content"""
    
    def __init__(self, file_path: Path):
        self.file_path = file_path
        self.content = file_path.read_text(encoding='utf-8')
        self.lines = self.content.splitlines(keepends=True)
    
    def extract_includes(self) -> Tuple[List[str], List[str]]:
        """Extract includes, separating unconditional and conditional ones"""
        unconditional_includes = []
        conditional_blocks = []
        
        i = 0
        while i < len(self.lines):
            line = self.lines[i]
            stripped = line.strip()
            
            # Check if we're entering a conditional block with includes (but not header guards)
            if (stripped.startswith('#ifdef') or stripped.startswith('#ifndef') or 
                stripped.startswith('#if defined')):
                # Skip if this is a header guard, but extract includes from within it
                if stripped.startswith('#ifndef') and self._is_header_guard(i):
                    # Extract includes from within the header guard, handling conditionals
                    j = i + 2  # Skip #ifndef and #define
                    while j < len(self.lines):
                        line = self.lines[j]
                        line_stripped = line.strip()
                        
                        # Check for conditional blocks within the header guard
                        if (line_stripped.startswith('#ifdef') or line_stripped.startswith('#ifndef') or 
                            line_stripped.startswith('#if defined')):
                            # Extract the conditional block
                            if self._block_contains_includes(j):
                                block = self._extract_conditional_block(j)
                                conditional_blocks.append(block)
                            j = self._skip_to_endif(j) + 1
                            continue
                        
                        # Extract unconditional includes
                        if line_stripped.startswith('#include'):
                            unconditional_includes.append(line.rstrip())
                        elif line_stripped.startswith('#endif'):
                            break
                        j += 1
                    i = self._skip_header_guard(i)
                    continue
                
                # Look ahead to see if this block contains includes
                if self._block_contains_includes(i):
                    block = self._extract_conditional_block(i)
                    conditional_blocks.append(block)
                    i = self._skip_to_endif(i) + 1
                    continue
            
            # Extract unconditional includes
            if stripped.startswith('#include'):
                unconditional_includes.append(line.rstrip())
            
            i += 1
        
        return unconditional_includes, conditional_blocks
    
    def extract_defines(self) -> List[str]:
        """Extract preprocessor defines, excluding blocks with includes"""
        defines = []
        i = 0
        
        while i < len(self.lines):
            line = self.lines[i]
            stripped = line.strip()
            
            # Skip header guards, but extract defines from within them
            if stripped.startswith('#ifndef') and self._is_header_guard(i):
                # Extract defines from within the header guard
                j = i + 2  # Skip #ifndef and #define
                while j < len(self.lines):
                    line = self.lines[j]
                    line_stripped = line.strip()
                    
                    # Check for conditional blocks within the header guard
                    if line_stripped.startswith('#ifdef') or line_stripped.startswith('#ifndef') or line_stripped.startswith('#if defined'):
                        # Skip if contains includes (handled separately)
                        if not self._block_contains_includes(j):
                            # Extract conditional block
                            if self._block_contains_defines_only(j):
                                block = self._extract_conditional_block(j)
                                defines.append(block)
                        j = self._skip_to_endif(j) + 1
                        continue
                    
                    # Extract simple defines (but skip multi-line macros - handled by extract_macros)
                    if line_stripped.startswith('#define') and not self._is_macro_definition(j):
                        defines.append(line.rstrip())
                    elif line_stripped.startswith('#endif'):
                        break
                    j += 1
                i = self._skip_header_guard(i)
                continue
            
            # Skip #pragma once
            if stripped == '#pragma once':
                i += 1
                continue
            
            # Skip standalone #endif that are likely header guard endings
            if stripped == '#endif' and self._is_likely_header_guard_end(i):
                i += 1
                continue
            
            # Handle conditional compilation blocks
            if stripped.startswith('#ifdef') or stripped.startswith('#if defined'):
                # Skip if contains includes (handled separately)
                if self._block_contains_includes(i):
                    i = self._skip_to_endif(i) + 1
                    continue
                
                # Only extract conditional blocks that contain defines, not function implementations
                if self._block_contains_defines_only(i):
                    block = self._extract_conditional_block(i)
                    defines.append(block)
                i = self._skip_to_endif(i) + 1
                continue
            
            # Handle #ifndef blocks (build config guards)
            if stripped.startswith('#ifndef'):
                # Skip if contains includes (handled separately)
                if self._block_contains_includes(i):
                    i = self._skip_to_endif(i) + 1
                    continue
                
                # Only extract conditional blocks that contain defines, not function implementations
                if self._block_contains_defines_only(i):
                    block = self._extract_conditional_block(i)
                    defines.append(block)
                i = self._skip_to_endif(i) + 1
                continue
            
            # Extract simple defines (but skip multi-line macros - handled by extract_macros)
            if stripped.startswith('#define') and not self._is_macro_definition(i):
                defines.append(line.rstrip())
            
            i += 1
        
        return defines
    
    def extract_namespace_content(self) -> List[str]:
        """Extract content from namespace trace blocks"""
        content = []
        i = 0
        while i < len(self.lines):
            line = self.lines[i]
            stripped = line.strip()
            
            # Skip header guards, but extract content from within them
            if stripped.startswith('#ifndef') and self._is_header_guard(i):
                # Extract namespace content from within the header guard
                j = i + 2  # Skip #ifndef and #define
                while j < len(self.lines):
                    line = self.lines[j]
                    line_stripped = line.strip()
                    
                    # Check for namespace content within the header guard
                    if line_stripped.startswith('namespace trace'):
                        # Extract the namespace content
                        j += 1  # Skip the opening line
                        brace_count = 1
                        while j < len(self.lines) and brace_count > 0:
                            line = self.lines[j]
                            line_stripped = line.strip()
                            
                            if '{' in line:
                                brace_count += line.count('{')
                            if '}' in line:
                                brace_count -= line.count('}')
                            
                            if brace_count > 0:
                                content.append(line.rstrip())
                            
                            j += 1
                        break
                    elif line_stripped.startswith('#endif'):
                        break
                    j += 1
                i = self._skip_header_guard(i)
                continue
            
            # Skip #pragma once
            if stripped == '#pragma once':
                i += 1
                continue
            
            # Skip includes and defines (handled separately)
            if (stripped.startswith('#include') or stripped.startswith('#ifndef') or
                stripped.startswith('#ifdef') or stripped.startswith('#define') or
                stripped.startswith('#endif') or stripped.startswith('#elif') or
                stripped.startswith('#else')):
                i += 1
                continue
            
            # Skip macros (handled separately)
            if stripped.startswith('#define') and self._is_macro_definition(i):
                i = self._skip_macro_definition(i)
                continue
            
            # Extract namespace content
            if stripped.startswith('namespace trace'):
                # Skip the opening line
                i += 1
                # Extract content until closing brace
                brace_count = 1
                while i < len(self.lines) and brace_count > 0:
                    line = self.lines[i]
                    stripped = line.strip()
                    
                    if '{' in line:
                        brace_count += line.count('{')
                    if '}' in line:
                        brace_count -= line.count('}')
                    
                    if brace_count > 0:
                        content.append(line.rstrip())
                    
                    i += 1
                continue
            
            i += 1
        
        return content
    
    def extract_macros(self) -> List[str]:
        """Extract macro definitions"""
        macros = []
        i = 0
        while i < len(self.lines):
            line = self.lines[i]
            stripped = line.strip()
            
            # Skip header guards, but extract macros from within them
            if stripped.startswith('#ifndef') and self._is_header_guard(i):
                # Extract macros from within the header guard
                j = i + 2  # Skip #ifndef and #define
                while j < len(self.lines):
                    line = self.lines[j]
                    line_stripped = line.strip()
                    
                    # Extract macro definitions
                    if line_stripped.startswith('#define') and self._is_macro_definition(j):
                        macro_lines = []
                        k = j
                        while k < len(self.lines):
                            macro_lines.append(self.lines[k].rstrip())
                            if not self.lines[k].rstrip().endswith('\\'):
                                break
                            k += 1
                        macros.append('\n'.join(macro_lines))
                        j = k + 1
                        continue
                    elif line_stripped.startswith('#endif'):
                        break
                    j += 1
                i = self._skip_header_guard(i)
                continue
            
            # Skip #pragma once
            if stripped == '#pragma once':
                i += 1
                continue
            
            # Extract macro definitions
            if stripped.startswith('#define') and self._is_macro_definition(i):
                macro_lines = []
                j = i
                while j < len(self.lines):
                    macro_lines.append(self.lines[j].rstrip())
                    if not self.lines[j].rstrip().endswith('\\'):
                        break
                    j += 1
                macros.append('\n'.join(macro_lines))
                i = j + 1
                continue
            
            i += 1
        
        return macros
    
    def _is_header_guard(self, i: int) -> bool:
        """Check if this is a header guard pattern"""
        if i + 1 >= len(self.lines):
            return False
        
        line1 = self.lines[i].strip()
        line2 = self.lines[i + 1].strip()
        
        if not line1.startswith('#ifndef'):
            return False
        
        guard_name = line1.split()[1] if len(line1.split()) > 1 else None
        if not guard_name:
            return False
        
        return line2.startswith(f'#define {guard_name}')
    
    def _is_header_guard_with_content(self, i: int) -> bool:
        """Check if this is a header guard that contains actual content (not just guards)"""
        if not self._is_header_guard(i):
            return False
        
        # Look for content between the guard and the final #endif
        j = i + 2  # Skip #ifndef and #define
        while j < len(self.lines):
            line = self.lines[j].strip()
            if line.startswith('#endif'):
                # Check if there's actual content between guard and endif
                for k in range(i + 2, j):
                    content_line = self.lines[k].strip()
                    if content_line and not content_line.startswith('//'):
                        return True
                return False
            j += 1
        
        return False
    
    def _skip_header_guard(self, i: int) -> int:
        """Skip header guard block and return new index, extracting content without guard wrapper"""
        # Skip #ifndef and #define lines
        i += 2
        
        # Find matching #endif at the end of file
        while i < len(self.lines):
            line = self.lines[i].strip()
            if line.startswith('#endif') and (i == len(self.lines) - 1 or 
                (i + 1 < len(self.lines) and not self.lines[i + 1].strip())):
                return i + 1
            i += 1
        
        return i
    
    def _is_macro_definition(self, i: int) -> bool:
        """Check if this is a macro definition (not a simple define)"""
        line = self.lines[i].strip()
        if not line.startswith('#define'):
            return False
        
        # Check if it's a function-like macro or multi-line macro
        return ('(' in line and ')' in line) or (i + 1 < len(self.lines) and self.lines[i + 1].strip().endswith('\\'))
    
    def _is_likely_header_guard_end(self, i: int) -> bool:
        """Check if this #endif is likely the end of a header guard"""
        # Check if this is at the end of the file or followed by empty lines
        if i == len(self.lines) - 1:
            return True
        
        # Check if followed by empty lines or comments
        for j in range(i + 1, len(self.lines)):
            line = self.lines[j].strip()
            if line and not line.startswith('//'):
                return False
            if line.startswith('//') and 'TRACE_SCOPE' in line:
                return True
        
        return True
    
    def _block_contains_includes(self, start_idx: int) -> bool:
        """Check if conditional block contains #include statements"""
        i = start_idx + 1
        depth = 1
        
        while i < len(self.lines) and depth > 0:
            line = self.lines[i].strip()
            if line.startswith('#include'):
                return True
            if line.startswith('#endif'):
                depth -= 1
            elif line.startswith('#ifdef') or line.startswith('#ifndef') or line.startswith('#if defined'):
                depth += 1
            i += 1
        
        return False
    
    def _block_contains_defines_only(self, start_idx: int) -> bool:
        """Check if conditional block contains only defines, not function implementations"""
        i = start_idx + 1
        depth = 1
        
        while i < len(self.lines) and depth > 0:
            line = self.lines[i].strip()
            
            # Check for function implementations (indicates this is not a defines-only block)
            if (line.startswith('inline ') or line.startswith('FILE* ') or 
                line.startswith('return ') or line.startswith('if ') or
                line.startswith('FILE* file') or 'fopen_s' in line or 'tmpfile_s' in line):
                return False
            
            # Check for namespace (indicates this is not a defines-only block)
            if line.startswith('namespace '):
                return False
            
            if line.startswith('#endif'):
                depth -= 1
            elif line.startswith('#ifdef') or line.startswith('#ifndef'):
                depth += 1
            i += 1
        
        return True
    
    def _extract_conditional_block(self, start_idx: int) -> str:
        """Extract complete conditional block as a string"""
        lines = []
        i = start_idx
        depth = 0
        
        while i < len(self.lines):
            line = self.lines[i]
            stripped = line.strip()
            
            if stripped.startswith('#ifdef') or stripped.startswith('#ifndef') or stripped.startswith('#if defined'):
                depth += 1
            elif stripped.startswith('#endif'):
                lines.append(line.rstrip())
                depth -= 1
                if depth == 0:
                    break
            
            lines.append(line.rstrip())
            i += 1
        
        return '\n'.join(lines)
    
    def _skip_to_endif(self, start_idx: int) -> int:
        """Skip to matching #endif and return its index"""
        i = start_idx + 1
        depth = 1
        
        while i < len(self.lines) and depth > 0:
            line = self.lines[i].strip()
            if line.startswith('#ifdef') or line.startswith('#ifndef') or line.startswith('#if defined'):
                depth += 1
            elif line.startswith('#endif'):
                depth -= 1
                if depth == 0:
                    return i
            i += 1
        
        return i

def analyze_macro_dependencies(macros: List[str]) -> List[str]:
    """Analyze macro dependencies and return ordered list"""
    # Extract macro names and their dependencies
    macro_info = {}
    
    for macro in macros:
        lines = macro.split('\n')
        first_line = lines[0].strip()
        
        # Extract macro name
        if first_line.startswith('#define'):
            parts = first_line.split()
            if len(parts) >= 2:
                macro_name = parts[1].split('(')[0]  # Remove function parameters
                
                # Find dependencies (other macros used in this macro)
                dependencies = set()
                for line in lines:
                    # Look for macro names in the macro body
                    for word in re.findall(r'\b[A-Z_][A-Z0-9_]*\b', line):
                        if word != macro_name and word in [name for name in macro_info.keys()]:
                            dependencies.add(word)
                
                macro_info[macro_name] = {
                    'definition': macro,
                    'dependencies': dependencies
                }
    
    # Topological sort to order dependencies first
    ordered = []
    visited = set()
    
    def visit(macro_name):
        if macro_name in visited:
            return
        visited.add(macro_name)
        
        if macro_name in macro_info:
            for dep in macro_info[macro_name]['dependencies']:
                visit(dep)
            ordered.append(macro_info[macro_name]['definition'])
    
    for macro_name in macro_info:
        visit(macro_name)
    
    return ordered

def deduplicate_includes(includes: List[str]) -> List[str]:
    """Deduplicate includes and sort them"""
    seen = set()
    system_includes = []
    project_includes = []
    
    for include in includes:
        if include not in seen:
            seen.add(include)
            if include.startswith('#include <'):
                system_includes.append(include)
            else:
                project_includes.append(include)
    
    # Sort system includes alphabetically
    system_includes.sort()
    project_includes.sort()
    
    return system_includes + project_includes

def deduplicate_defines(defines: List[str]) -> List[str]:
    """Deduplicate defines, keeping first occurrence"""
    seen = set()
    result = []
    
    for define in defines:
        # Extract the define name for deduplication
        if define.startswith('#define'):
            parts = define.split()
            if len(parts) >= 2:
                define_name = parts[1].split('(')[0]  # Remove function parameters
                if define_name not in seen:
                    seen.add(define_name)
                    result.append(define)
        else:
            # Keep non-define preprocessor directives
            result.append(define)
    
    return result

def deduplicate_conditional_blocks(blocks: List[str]) -> List[str]:
    """Deduplicate conditional compilation blocks"""
    seen = set()
    result = []
    
    for block in blocks:
        # Normalize whitespace for comparison
        normalized = ' '.join(block.split())
        if normalized not in seen:
            seen.add(normalized)
            result.append(block)
    
    return result

def deduplicate_namespace_content(content: List[str]) -> List[str]:
    """Remove duplicate forward declarations"""
    seen_declarations = set()
    result = []
    
    for line in content:
        stripped = line.strip()
        
        # Check if this is a forward declaration
        if (stripped.startswith('struct ') and stripped.endswith(';') or
            stripped.startswith('enum class ') and stripped.endswith(';') or
            stripped.startswith('class ') and stripped.endswith(';')):
            
            if stripped not in seen_declarations:
                seen_declarations.add(stripped)
                result.append(line)
        else:
            result.append(line)
    
    return result

def read_modules_list(input_dir: Path) -> List[str]:
    """Read modules.txt to get ordered list of files"""
    modules_file = input_dir / "modules.txt"
    
    if not modules_file.exists():
        raise FileNotFoundError(f"modules.txt not found in {input_dir}")
    
    modules = []
    with open(modules_file, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip empty lines and comments
            if line and not line.startswith('#'):
                modules.append(line)
    
    return modules

def generate_header(input_dir: Path, output_file: Path):
    """Generate merged header file with proper structure"""
    
    input_dir = Path(input_dir)
    output_file = Path(output_file)
    
    # Read module list
    modules = read_modules_list(input_dir)
    
    # Generate header comment
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S UTC')
    
    header_comment = f"""#pragma once
/**
 * @file trace_scope.hpp
 * @brief Single-header trace library (auto-generated)
 * 
 * This file is automatically generated from modular sources.
 * DO NOT EDIT THIS FILE DIRECTLY - edit the source modules instead.
 * 
 * Generated: {timestamp}
 * Source modules: {len(modules)} files
 * 
 * Source files (in merge order):
"""
    
    for module in modules:
        header_comment += f" *   - {module}\n"
    
    header_comment += """ * 
 * To regenerate this file:
 *   python tools/merge_header.py --input include/trace-scope/trace_scope_modular --output include/trace-scope/trace_scope.hpp
 * 
 * Or use CMake (automatic):
 *   cmake --build . --target generate_header
 */

"""
    
    # Phase 1: Collect content from all modules
    all_unconditional_includes = []
    all_conditional_include_blocks = []
    all_defines = []
    all_namespace_content = []
    all_macros = []
    
    for module in modules:
        module_path = input_dir / module
        
        if not module_path.exists():
            raise FileNotFoundError(f"Module not found: {module_path}")
        
        # Parse and extract content
        parser = HeaderParser(module_path)
        uncond_includes, cond_blocks = parser.extract_includes()
        all_unconditional_includes.extend(uncond_includes)
        all_conditional_include_blocks.extend(cond_blocks)
        all_defines.extend(parser.extract_defines())
        all_namespace_content.extend(parser.extract_namespace_content())
        all_macros.extend(parser.extract_macros())
    
    # Phase 2: Deduplicate and organize
    includes = deduplicate_includes(all_unconditional_includes)
    conditional_blocks = deduplicate_conditional_blocks(all_conditional_include_blocks)
    defines = deduplicate_defines(all_defines)
    namespace_content = deduplicate_namespace_content(all_namespace_content)
    macros = analyze_macro_dependencies(all_macros)
    
    # Phase 3: Generate output
    merged_content = header_comment
    
    # Add unconditional includes
    if includes:
        merged_content += "// Standard C++ includes\n"
        for include in includes:
            merged_content += f"{include}\n"
        merged_content += "\n"
    
    # Add conditional include blocks
    if conditional_blocks:
        merged_content += "// Platform-specific includes\n"
        for block in conditional_blocks:
            merged_content += f"{block}\n"
        merged_content += "\n"
    
    # Add defines
    if defines:
        merged_content += "// Build config defines\n"
        for define in defines:
            merged_content += f"{define}\n"
        merged_content += "\n"
    
    # Add namespace
    merged_content += "// Single namespace\n"
    merged_content += "namespace trace {\n\n"
    
    # Add namespace content
    for line in namespace_content:
        merged_content += f"{line}\n"
    
    merged_content += "\n} // namespace trace\n\n"
    
    # Add macros
    if macros:
        merged_content += "// Macros (outside namespace)\n"
        for macro in macros:
            merged_content += f"{macro}\n"
        merged_content += "\n"
    
    # Write output
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(merged_content, encoding='utf-8')
    
    print(f"✓ Generated {output_file}")
    print(f"  Merged {len(modules)} modules")
    print(f"  Total size: {len(merged_content):,} bytes")
    print(f"  Total lines: {len(merged_content.splitlines()):,}")

def main():
    parser = argparse.ArgumentParser(
        description='Merge modular C++ header files into single header',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python merge_header.py --input include/trace-scope/trace_scope_modular --output include/trace-scope/trace_scope.hpp
  python merge_header.py -i include/trace-scope/trace_scope_modular -o include/trace-scope/trace_scope.hpp
        """
    )
    
    parser.add_argument('-i', '--input', required=True,
                       help='Input directory containing module headers')
    parser.add_argument('-o', '--output', required=True,
                       help='Output path for merged header file')
    
    args = parser.parse_args()
    
    try:
        generate_header(args.input, args.output)
    except Exception as e:
        print(f"Error: {e}")
        return 1
    
    return 0

if __name__ == '__main__':
    exit(main())