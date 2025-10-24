#!/bin/bash
set -e

echo "Running clang-tidy checks (this may take 2-5 minutes)..."
cmake --preset lint

# Build with timeout and parallel jobs
timeout 600 cmake --build --preset lint -j4 2>&1 | tee lint_output.txt

echo ""
echo "Linting complete. Results saved to lint_output.txt"
