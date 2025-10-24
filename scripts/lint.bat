@echo off
echo Running clang-tidy checks (this may take 2-5 minutes)...
cmake --preset lint

REM Build with parallel jobs
cmake --build --preset lint -j4 2>&1 | tee lint_output.txt

echo.
echo Linting complete. Results saved to lint_output.txt
