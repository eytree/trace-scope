cmake_minimum_required(VERSION 3.16)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TRACE_SCOPE_VERSION)
string(STRIP "${TRACE_SCOPE_VERSION}" TRACE_SCOPE_VERSION)

# Extract numeric version for CMake (CMake VERSION doesn't support suffixes like "-alpha")
string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" TRACE_SCOPE_VERSION_NUMERIC "${TRACE_SCOPE_VERSION}")

project(trace_scope 
    VERSION ${TRACE_SCOPE_VERSION_NUMERIC}
    LANGUAGES CXX
)

option(TRACE_ENABLED "Enable trace instrumentation" ON)
option(TRACE_DOUBLE_BUFFER "Enable double-buffering support (saves ~1.2MB per thread when OFF)" OFF)
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

if(TRACE_DOUBLE_BUFFER)
    target_compile_definitions(trace_scope INTERFACE TRACE_DOUBLE_BUFFER=1)
else()
    target_compile_definitions(trace_scope INTERFACE TRACE_DOUBLE_BUFFER=0)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
