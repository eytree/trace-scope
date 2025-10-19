cmake_minimum_required(VERSION 3.16)
project(trace_scope LANGUAGES CXX)

option(TRACE_ENABLED "Enable trace instrumentation" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Interface header-only library
add_library(trace_scope INTERFACE)
target_include_directories(trace_scope INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
if(TRACE_ENABLED)
    target_compile_definitions(trace_scope INTERFACE TRACE_ENABLED=1)
else()
    target_compile_definitions(trace_scope INTERFACE TRACE_ENABLED=0)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
