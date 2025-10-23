# Example DLL infrastructure for trace-scope
# This creates a real DLL and executable to demonstrate DLL state sharing

# Build the example DLL (shared library)
add_library(my_library SHARED my_library.cpp)
target_link_libraries(my_library PRIVATE trace_scope)
target_compile_definitions(my_library PRIVATE MY_LIBRARY_EXPORTS)

# Build the example executable
add_executable(dll_main dll_main.cpp)
target_link_libraries(dll_main PRIVATE trace_scope my_library)

# Ensure DLL is in the same directory as the executable for Windows
# This is critical for Windows DLL loading
set_target_properties(my_library PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:dll_main>
)

# On Windows, also copy the DLL to the build directory for easy testing
if(WIN32)
    add_custom_command(TARGET dll_main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:my_library>
        $<TARGET_FILE_DIR:dll_main>
        COMMENT "Copying DLL to executable directory"
    )
endif()

# Set output names for clarity
set_target_properties(my_library PROPERTIES OUTPUT_NAME "my_library")
set_target_properties(dll_main PROPERTIES OUTPUT_NAME "dll_main")
