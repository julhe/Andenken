@echo off
setlocal

cmake . -B ./build 
set exit_code=%errorlevel%

if %exit_code% neq 0 (
    pause
)

endlocal