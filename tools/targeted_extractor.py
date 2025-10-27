#!/usr/bin/env python3
"""
targeted_extractor.py - Extract specific functions and types from original header

This tool extracts specific missing pieces from the original header to fill in
the clean manual structure.
"""

import argparse
import re
from pathlib import Path

class TargetedExtractor:
    """Extract specific functions and types from original header"""
    
    def __init__(self, original_file: str, output_file: str):
        self.original_file = Path(original_file)
        self.output_file = Path(output_file)
        self.content = self.original_file.read_text(encoding='utf-8')
    
    def extract_missing_pieces(self):
        """Extract missing functions and types"""
        print(f"Extracting missing pieces from {self.original_file} to {self.output_file}")
        
        # Read the current clean header
        clean_content = self.output_file.read_text(encoding='utf-8')
        
        # Extract specific missing pieces
        missing_pieces = []
        
        # Extract TraceStream class
        trace_stream = self._extract_trace_stream()
        if trace_stream:
            missing_pieces.append(trace_stream)
        
        # Extract trace_msgf function
        trace_msgf = self._extract_trace_msgf()
        if trace_msgf:
            missing_pieces.append(trace_msgf)
        
        # Extract safe_fopen function
        safe_fopen = self._extract_safe_fopen()
        if safe_fopen:
            missing_pieces.append(safe_fopen)
        
        # Extract flush_ring function
        flush_ring = self._extract_flush_ring()
        if flush_ring:
            missing_pieces.append(flush_ring)
        
        # Extract thread_ring function
        thread_ring = self._extract_thread_ring()
        if thread_ring:
            missing_pieces.append(thread_ring)
        
        # Extract flush_all function
        flush_all = self._extract_flush_all()
        if flush_all:
            missing_pieces.append(flush_all)
        
        # Extract config.out field
        config_out = self._extract_config_out()
        if config_out:
            missing_pieces.append(config_out)
        
        # Insert missing pieces into clean header
        if missing_pieces:
            # Find the end of the namespace trace block
            namespace_end = clean_content.rfind('} // namespace trace')
            if namespace_end != -1:
                # Insert before the closing brace
                before_end = clean_content[:namespace_end]
                after_end = clean_content[namespace_end:]
                
                # Add missing pieces
                new_content = before_end + '\n\n' + '\n\n'.join(missing_pieces) + '\n' + after_end
                
                # Write updated content
                self.output_file.write_text(new_content, encoding='utf-8')
                print(f"✓ Added {len(missing_pieces)} missing pieces to {self.output_file}")
            else:
                print("✗ Could not find namespace end to insert missing pieces")
        else:
            print("No missing pieces found")
    
    def _extract_trace_stream(self):
        """Extract TraceStream class"""
        # Look for TraceStream class definition
        pattern = r'class TraceStream[^{]*\{[^}]*\};'
        match = re.search(pattern, self.content, re.DOTALL)
        if match:
            return f"// TraceStream class\ntrace::{match.group(0)}"
        return None
    
    def _extract_trace_msgf(self):
        """Extract trace_msgf function"""
        # Look for trace_msgf function
        pattern = r'inline void trace_msgf\([^)]*\)[^{]*\{[^}]*\}'
        match = re.search(pattern, self.content, re.DOTALL)
        if match:
            return f"// trace_msgf function\n{match.group(0)}"
        return None
    
    def _extract_safe_fopen(self):
        """Extract safe_fopen function"""
        # Look for safe_fopen function
        pattern = r'inline FILE\* safe_fopen\([^)]*\)[^{]*\{[^}]*\}'
        match = re.search(pattern, self.content, re.DOTALL)
        if match:
            return f"// safe_fopen function\n{match.group(0)}"
        return None
    
    def _extract_flush_ring(self):
        """Extract flush_ring function"""
        # Look for flush_ring function
        pattern = r'inline void flush_ring\([^)]*\)[^{]*\{[^}]*\}'
        match = re.search(pattern, self.content, re.DOTALL)
        if match:
            return f"// flush_ring function\n{match.group(0)}"
        return None
    
    def _extract_thread_ring(self):
        """Extract thread_ring function"""
        # Look for thread_ring function
        pattern = r'inline Ring\* thread_ring\([^)]*\)[^{]*\{[^}]*\}'
        match = re.search(pattern, self.content, re.DOTALL)
        if match:
            return f"// thread_ring function\n{match.group(0)}"
        return None
    
    def _extract_flush_all(self):
        """Extract flush_all function"""
        # Look for flush_all function
        pattern = r'inline void flush_all\([^)]*\)[^{]*\{[^}]*\}'
        match = re.search(pattern, self.content, re.DOTALL)
        if match:
            return f"// flush_all function\n{match.group(0)}"
        return None
    
    def _extract_config_out(self):
        """Extract config.out field"""
        # Look for output_file field in Config struct
        pattern = r'FILE\*\s+output_file[^;]*;'
        match = re.search(pattern, self.content)
        if match:
            return f"// Config output_file field\n    {match.group(0)}"
        return None

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='Extract specific functions from original header')
    parser.add_argument('--original', '-i', required=True, help='Original header file')
    parser.add_argument('--output', '-o', required=True, help='Output header file to update')
    
    args = parser.parse_args()
    
    extractor = TargetedExtractor(args.original, args.output)
    extractor.extract_missing_pieces()

if __name__ == "__main__":
    main()
