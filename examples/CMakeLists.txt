# Examples for trace-scope library

add_executable(example_basic example_basic.cpp)
target_link_libraries(example_basic PRIVATE trace_scope)

add_executable(example_dll_shared example_dll_shared.cpp)
target_link_libraries(example_dll_shared PRIVATE trace_scope)

add_executable(example_custom_markers example_custom_markers.cpp)
target_link_libraries(example_custom_markers PRIVATE trace_scope)

add_executable(example_colors example_colors.cpp)
target_link_libraries(example_colors PRIVATE trace_scope)

add_executable(example_hybrid example_hybrid.cpp)
target_link_libraries(example_hybrid PRIVATE trace_scope)

# Double-buffer example only built when TRACE_DOUBLE_BUFFER is enabled
if(TRACE_DOUBLE_BUFFER)
    add_executable(example_double_buffer example_double_buffer.cpp)
    target_link_libraries(example_double_buffer PRIVATE trace_scope)
endif()

add_executable(example_config_file example_config_file.cpp)
target_link_libraries(example_config_file PRIVATE trace_scope)

add_executable(example_filtering example_filtering.cpp)
target_link_libraries(example_filtering PRIVATE trace_scope)

add_executable(example_thread_colors example_thread_colors.cpp)
target_link_libraries(example_thread_colors PRIVATE trace_scope)

add_executable(test_python_tool test_python_tool.cpp)
target_link_libraries(test_python_tool PRIVATE trace_scope)

add_executable(example_stats example_stats.cpp)
target_link_libraries(example_stats PRIVATE trace_scope)

