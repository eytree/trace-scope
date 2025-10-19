# Tests for trace-scope library

# Test framework self-test (tests the test framework itself)
add_executable(test_framework_test test_framework_test.cpp)
# No trace_scope dependency - tests the framework in isolation

add_executable(test_trace test_trace.cpp)
target_link_libraries(test_trace PRIVATE trace_scope)

add_executable(test_comprehensive test_comprehensive.cpp)
target_link_libraries(test_comprehensive PRIVATE trace_scope)

add_executable(test_binary_format test_binary_format.cpp)
target_link_libraries(test_binary_format PRIVATE trace_scope)

# Double-buffer test only built when TRACE_DOUBLE_BUFFER is enabled
if(TRACE_DOUBLE_BUFFER)
    add_executable(test_double_buffer test_double_buffer.cpp)
    target_link_libraries(test_double_buffer PRIVATE trace_scope)
endif()

add_executable(test_config_file test_config_file.cpp)
target_link_libraries(test_config_file PRIVATE trace_scope)

add_executable(test_filtering test_filtering.cpp)
target_link_libraries(test_filtering PRIVATE trace_scope)

