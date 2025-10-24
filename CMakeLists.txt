cmake_minimum_required(VERSION 3.16)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TRC_SCOPE_VERSION)
string(STRIP "${TRC_SCOPE_VERSION}" TRC_SCOPE_VERSION)

# Extract numeric version for CMake (CMake VERSION doesn't support suffixes like "-alpha")
string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" TRC_SCOPE_VERSION_NUMERIC "${TRC_SCOPE_VERSION}")

project(trace_scope 
    VERSION ${TRC_SCOPE_VERSION_NUMERIC}
    LANGUAGES CXX
)

option(TRC_ENABLED "Enable trace instrumentation" ON)
option(TRC_DOUBLE_BUFFER "Enable double-buffering support (saves ~1.2MB per thread when OFF)" OFF)
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
if(TRC_ENABLED)
    target_compile_definitions(trace_scope INTERFACE TRC_ENABLED=1)
else()
    target_compile_definitions(trace_scope INTERFACE TRC_ENABLED=0)
endif()

if(TRC_DOUBLE_BUFFER)
    target_compile_definitions(trace_scope INTERFACE TRC_DOUBLE_BUFFER=1)
else()
    target_compile_definitions(trace_scope INTERFACE TRC_DOUBLE_BUFFER=0)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
