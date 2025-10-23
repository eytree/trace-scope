# Test DLL infrastructure for trace-scope
# This creates a real DLL and executable to test DLL state sharing

# Build the test DLL (shared library)
add_library(test_dll_library SHARED test_dll_library.cpp)
target_link_libraries(test_dll_library PRIVATE trace_scope)
target_compile_definitions(test_dll_library PRIVATE TEST_DLL_EXPORTS)

# Build the test executable
add_executable(test_dll_main test_dll_main.cpp)
target_link_libraries(test_dll_main PRIVATE trace_scope test_dll_library)

# Ensure DLL is in the same directory as the executable for Windows
# This is critical for Windows DLL loading
set_target_properties(test_dll_library PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:test_dll_main>
)

# On Windows, also copy the DLL to the build directory for easy testing
if(WIN32)
    add_custom_command(TARGET test_dll_main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:test_dll_library>
        $<TARGET_FILE_DIR:test_dll_main>
        COMMENT "Copying DLL to executable directory"
    )
endif()

# Set output names for clarity
set_target_properties(test_dll_library PROPERTIES OUTPUT_NAME "test_dll_library")
set_target_properties(test_dll_main PROPERTIES OUTPUT_NAME "test_dll_main")
