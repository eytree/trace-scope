#!/usr/bin/env python3
"""
Unit tests for trc_pretty.py

Run with: python test_trc_pretty.py
"""

import unittest
import tempfile
import struct
import os
import sys
from io import StringIO

# Import the module we're testing
sys.path.insert(0, os.path.dirname(__file__))
from trc_pretty import read_str, format_duration, main


class TestTrcPretty(unittest.TestCase):
    """Test cases for trc_pretty.py"""
    
    def test_format_duration_nanoseconds(self):
        """Test duration formatting for nanoseconds."""
        self.assertEqual(format_duration(500), '500 ns')
        self.assertEqual(format_duration(999), '999 ns')
    
    def test_format_duration_microseconds(self):
        """Test duration formatting for microseconds."""
        self.assertEqual(format_duration(1500), '1.50 us')
        self.assertEqual(format_duration(999999), '1000.00 us')
    
    def test_format_duration_milliseconds(self):
        """Test duration formatting for milliseconds."""
        self.assertEqual(format_duration(1500000), '1.50 ms')
        self.assertEqual(format_duration(999999999), '1000.00 ms')
    
    def test_format_duration_seconds(self):
        """Test duration formatting for seconds."""
        self.assertEqual(format_duration(1500000000), '1.500 s')
        self.assertEqual(format_duration(5234567890), '5.235 s')
    
    def test_read_length_prefixed_string(self):
        """Test reading length-prefixed strings."""
        # Create a binary string: length(2) + "hello"
        data = struct.pack('<H', 5) + b'hello'
        
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(data)
            temp_path = f.name
        
        try:
            with open(temp_path, 'rb') as f:
                result = read_str(f)
                self.assertEqual(result, 'hello')
        finally:
            os.unlink(temp_path)
    
    def test_read_empty_string(self):
        """Test reading empty length-prefixed string."""
        data = struct.pack('<H', 0)
        
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(data)
            temp_path = f.name
        
        try:
            with open(temp_path, 'rb') as f:
                result = read_str(f)
                self.assertEqual(result, '')
        finally:
            os.unlink(temp_path)
    
    def create_test_binary(self, events):
        """Helper to create a test binary file with given events."""
        with tempfile.NamedTemporaryFile(delete=False, suffix='.bin') as f:
            # Write header
            f.write(b'TRCLOG10')
            f.write(struct.pack('<II', 1, 0))  # version=1, padding=0
            
            # Write events
            for event in events:
                typ, tid, ts_ns, depth, dur_ns, file_str, func_str, msg_str, line = event
                
                f.write(struct.pack('<B', typ))
                f.write(struct.pack('<I', tid))
                f.write(struct.pack('<Q', ts_ns))
                f.write(struct.pack('<I', depth))
                f.write(struct.pack('<Q', dur_ns))
                
                # Write strings
                for s in [file_str, func_str, msg_str]:
                    s_bytes = s.encode('utf-8') if s else b''
                    f.write(struct.pack('<H', len(s_bytes)))
                    if s_bytes:
                        f.write(s_bytes)
                
                f.write(struct.pack('<I', line))
            
            return f.name
    
    def test_parse_enter_event(self):
        """Test parsing a function entry event."""
        events = [
            (0, 0x12345678, 1000000, 1, 0, 'test.cpp', 'my_function', '', 42)
        ]
        
        bin_file = self.create_test_binary(events)
        
        try:
            # Capture output
            old_stdout = sys.stdout
            sys.stdout = StringIO()
            
            main(bin_file)
            
            output = sys.stdout.getvalue()
            sys.stdout = old_stdout
            
            # Verify output
            self.assertIn('12345678', output)  # thread ID
            self.assertIn('my_function', output)
            self.assertIn('test.cpp:42', output)
            self.assertIn('->', output)  # Enter marker
        finally:
            os.unlink(bin_file)
    
    def test_parse_exit_event(self):
        """Test parsing a function exit event."""
        events = [
            (1, 0xAABBCCDD, 2000000, 1, 150000, 'test.cpp', 'my_function', '', 42)
        ]
        
        bin_file = self.create_test_binary(events)
        
        try:
            old_stdout = sys.stdout
            sys.stdout = StringIO()
            
            main(bin_file)
            
            output = sys.stdout.getvalue()
            sys.stdout = old_stdout
            
            # Verify output
            self.assertIn('aabbccdd', output)  # thread ID in hex
            self.assertIn('my_function', output)
            self.assertIn('<-', output)  # Exit marker
            self.assertIn('us', output)  # Duration in microseconds
        finally:
            os.unlink(bin_file)
    
    def test_parse_message_event(self):
        """Test parsing a message event."""
        events = [
            (2, 0x11111111, 3000000, 2, 0, 'test.cpp', 'parent_func', 'Test message', 99)
        ]
        
        bin_file = self.create_test_binary(events)
        
        try:
            old_stdout = sys.stdout
            sys.stdout = StringIO()
            
            main(bin_file)
            
            output = sys.stdout.getvalue()
            sys.stdout = old_stdout
            
            # Verify output
            self.assertIn('Test message', output)
            self.assertIn('-', output)  # Message marker
            self.assertIn('test.cpp:99', output)
        finally:
            os.unlink(bin_file)
    
    def test_parse_multiple_events(self):
        """Test parsing multiple events."""
        events = [
            (0, 0x12345678, 1000000, 1, 0, 'test.cpp', 'func1', '', 10),
            (2, 0x12345678, 1001000, 2, 0, 'test.cpp', 'func1', 'Message', 15),
            (1, 0x12345678, 1002000, 1, 2000, 'test.cpp', 'func1', '', 10),
        ]
        
        bin_file = self.create_test_binary(events)
        
        try:
            old_stderr = sys.stderr
            sys.stderr = StringIO()
            
            main(bin_file)
            
            error_output = sys.stderr.getvalue()
            sys.stderr = old_stderr
            
            # Should process 3 events
            self.assertIn('3 events', error_output)
        finally:
            os.unlink(bin_file)


def run_tests():
    """Run all tests."""
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    
    suite.addTests(loader.loadTestsFromTestCase(TestTrcPretty))
    
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    
    print("\n" + "="*60)
    if result.wasSuccessful():
        print("✓ All trace_instrument tests passed!")
    else:
        print(f"✗ {len(result.failures)} failure(s), {len(result.errors)} error(s)")
    print("="*60)
    
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(run_tests())

