#!/usr/bin/env python3
"""
Test libclang installation and basic functionality
"""

import clang.cindex

def test_libclang():
    """Test libclang installation"""
    try:
        print(f"libclang module loaded successfully")
        print(f"Configuration: {clang.cindex.conf}")
        
        # Test basic parsing
        index = clang.cindex.Index.create()
        print("✓ Index created successfully")
        
        # Try to parse a simple C++ file
        test_content = """
        #include <iostream>
        
        namespace test {
            void hello() {
                std::cout << "Hello, World!" << std::endl;
            }
        }
        """
        
        # Write test file
        with open("test_cpp.cpp", "w") as f:
            f.write(test_content)
        
        # Parse it
        tu = index.parse("test_cpp.cpp", args=['-x', 'c++', '-std=c++17'])
        
        if tu.diagnostics:
            print("Diagnostics:")
            for diag in tu.diagnostics:
                print(f"  {diag.severity}: {diag.spelling}")
        else:
            print("✓ Parsing successful, no diagnostics")
        
        # Test AST traversal
        namespace_found = False
        function_found = False
        
        for cursor in tu.cursor.walk_preorder():
            if cursor.kind == clang.cindex.CursorKind.NAMESPACE:
                if cursor.spelling == "test":
                    namespace_found = True
                    print(f"✓ Found namespace: {cursor.spelling}")
            elif cursor.kind == clang.cindex.CursorKind.FUNCTION_DECL:
                if cursor.spelling == "hello":
                    function_found = True
                    print(f"✓ Found function: {cursor.spelling}")
        
        if namespace_found and function_found:
            print("✓ AST traversal working correctly")
        else:
            print("✗ AST traversal issues")
        
        # Clean up
        import os
        os.remove("test_cpp.cpp")
        
    except Exception as e:
        print(f"✗ Error: {e}")
        return False
    
    return True

if __name__ == "__main__":
    print("Testing libclang installation...")
    success = test_libclang()
    if success:
        print("\n✓ libclang is working correctly!")
    else:
        print("\n✗ libclang installation has issues")
