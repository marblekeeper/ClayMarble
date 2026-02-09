@echo off
setlocal

REM === Configuration (Matches your MarbleEngine setup) ===
set "MSYS_DIR=C:\msys64\ucrt64"
set "PATH=%MSYS_DIR%\bin;C:\msys64\usr\bin;%PATH%"

echo [1/2] Compiling version.c using GCC...
gcc -std=c99 version.c -o version.exe

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed. Check if version.c exists in this folder.
    exit /b 1
)

echo [2/2] Running version.exe...
echo ---------------------------------
if exist version.exe (
    .\version.exe
)
echo ---------------------------------

pause