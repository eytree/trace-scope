@echo off
echo Running clang-tidy checks...
cmake --preset lint
cmake --build --preset lint 2>&1 | tee lint_output.txt

echo.
echo Linting complete. Results saved to lint_output.txt
