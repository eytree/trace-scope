@echo off
if "%~1"=="" (
    set FILES=include/trace-scope/trace_scope.hpp
) else (
    set FILES=%*
)

for %%f in (%FILES%) do (
    echo Checking %%f...
    clang-tidy "%%f" --config-file=.clang-tidy --header-filter=include/trace-scope/.* -- -std=c++17 -Iinclude
)
