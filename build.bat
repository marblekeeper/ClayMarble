@echo off
setlocal EnableDelayedExpansion

REM === Configuration ===
set "MSYS_DIR=C:\msys64\ucrt64"
set "PATH=%MSYS_DIR%\bin;C:\msys64\usr\bin;%PATH%"
set "OUT_NAME=marble_phase0_2.exe"

REM === Detect Lua Library Name ===
set "LUA_LIB=lua"
if exist "%MSYS_DIR%\lib\liblua54.a" set "LUA_LIB=lua54"

REM === Pre-build Cleanup ===
taskkill /F /IM %OUT_NAME% >nul 2>nul
taskkill /F /IM test.exe >nul 2>nul
taskkill /F /IM test_cmd.exe >nul 2>nul
if exist %OUT_NAME% del /Q %OUT_NAME%
if exist test.exe del /Q test.exe
if exist test_cmd.exe del /Q test_cmd.exe

REM --- ROUTE: TEST ---
if "%1"=="test" goto DO_TEST
if "%1"=="gcc" goto DO_GCC

:USAGE
echo Usage: build.bat [gcc^|test]
exit /b 1

:DO_TEST
echo [1/3] Building Core Tests (GCC)...
gcc -std=c99 -w -O2 test.c -o test.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm

echo [2/3] Building Command Tests (GCC)...
gcc -std=c99 -w -O2 test_cmd.c -o test_cmd.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm

echo.
echo [3/3] Running Test Suite...
echo ----------------------------------------
if exist test.exe (
    echo Running Core:
    .\test.exe
) else (
    echo [ERROR] test.exe was not created.
)

echo.
if exist test_cmd.exe (
    echo Running Command Buffer:
    .\test_cmd.exe
) else (
    echo [ERROR] test_cmd.exe was not created.
    echo Attempted link: -l%LUA_LIB%
)
exit /b 0

:DO_GCC
echo Building with GCC...
gcc -std=c99 -w -O2 main.c -o %OUT_NAME% -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm

if exist %OUT_NAME% (
    echo Build Success: %OUT_NAME%
) else (
    echo [ERROR] Build failed.
)
exit /b 0