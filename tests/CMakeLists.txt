# Tests for trace-scope library

add_executable(test_trace test_trace.cpp)
target_link_libraries(test_trace PRIVATE trace_scope)

add_executable(test_comprehensive test_comprehensive.cpp)
target_link_libraries(test_comprehensive PRIVATE trace_scope)

add_executable(test_binary_format test_binary_format.cpp)
target_link_libraries(test_binary_format PRIVATE trace_scope)

