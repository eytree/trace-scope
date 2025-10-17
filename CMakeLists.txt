cmake_minimum_required(VERSION 3.16)
project(trace_scope LANGUAGES CXX)

option(TRACE_ENABLED "Enable trace instrumentation" ON)
option(BUILD_TESTS "Build tests" ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Interface header-only library
add_library(trace_scope INTERFACE)
target_include_directories(trace_scope INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(TRACE_ENABLED)
    target_compile_definitions(trace_scope INTERFACE TRACE_ENABLED=1)
else()
    target_compile_definitions(trace_scope INTERFACE TRACE_ENABLED=0)
endif()

# Example
add_executable(example_basic examples/example_basic.cpp)
target_link_libraries(example_basic PRIVATE trace_scope)

# Tests
if(BUILD_TESTS)
  add_executable(test_trace tests/test_trace.cpp)
  target_link_libraries(test_trace PRIVATE trace_scope)
  
  add_executable(test_comprehensive tests/test_comprehensive.cpp)
  target_link_libraries(test_comprehensive PRIVATE trace_scope)
endif()
