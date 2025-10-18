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

