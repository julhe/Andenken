@echo off
setlocal

cmake . -B ./buildDevelopment
set exit_code=%errorlevel%

if %exit_code% neq 0 (
    pause
)

endlocal