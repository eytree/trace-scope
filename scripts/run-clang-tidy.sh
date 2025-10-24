#!/bin/bash
# Run clang-tidy on specific files without building

FILES=${@:-"include/trace-scope/trace_scope.hpp"}

for file in $FILES; do
    echo "Checking $file..."
    clang-tidy "$file" \
        --config-file=.clang-tidy \
        --header-filter='include/trace-scope/.*' \
        -- -std=c++17 -Iinclude
done
