@echo off
setlocal

:: =========================================================================
:: MARBLE ENGINE: NETWORK TEST RUNNER
:: Usage: network.bat [win|web|clean]
:: =========================================================================

set SRC=test_net.c
set OUT_WIN=test_net.exe
set OUT_WEB=test_net.js

:: Check input arguments
if /i "%1"=="win" goto build_win
if /i "%1"=="web" goto build_web
if /i "%1"=="clean" goto clean
goto help

:: =========================================================================
:: WINDOWS TARGET (GCC)
:: =========================================================================
:build_win
echo [WIN] Checking for GCC...
where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: GCC not found in PATH. Please install MinGW or similar.
    exit /b 1
)

echo [WIN] Compiling %SRC%...
gcc -std=c99 -Wall -Wextra -O2 %SRC% -o %OUT_WIN%
if %errorlevel% neq 0 (
    echo [WIN] Compilation FAILED.
    exit /b 1
)

echo [WIN] Compilation SUCCESS. Running Unit Tests...
echo ---------------------------------------------------
%OUT_WIN%
echo ---------------------------------------------------
echo.
echo To run the interactive demo, type: %OUT_WIN% --demo
goto end

:: =========================================================================
:: WEB TARGET (EMSCRIPTEN)
:: =========================================================================
:build_web
echo [WEB] Checking for Emscripten (emcc)...
call emcc --version >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: emcc not found in PATH. Please activate Emscripten environment.
    exit /b 1
)

echo [WEB] Compiling %SRC% to WASM/JS...
:: We compile to .js so it can be run via Node for testing or embedded in HTML
call emcc -std=c99 -Wall -Wextra -O2 %SRC% -o %OUT_WEB%
if %errorlevel% neq 0 (
    echo [WEB] Compilation FAILED.
    exit /b 1
)

echo [WEB] Compilation SUCCESS (%OUT_WEB% generated).
echo.
echo Checking for Node.js to run tests...
where node >nul 2>nul
if %errorlevel% equ 0 (
    echo [WEB] Running via Node.js...
    echo ---------------------------------------------------
    node %OUT_WEB%
    echo ---------------------------------------------------
) else (
    echo [WEB] Node.js not found. Open %OUT_WEB% in a browser or install Node to run tests headless.
)
goto end

:: =========================================================================
:: CLEAN
:: =========================================================================
:clean
echo Cleaning build artifacts...
if exist %OUT_WIN% del %OUT_WIN%
if exist %OUT_WEB% del %OUT_WEB%
if exist test_net.wasm del test_net.wasm
echo Done.
goto end

:: =========================================================================
:: HELP
:: =========================================================================
:help
echo.
echo Usage: network.bat [target]
echo.
echo Targets:
echo   win    - Compile with GCC and run unit tests (Windows)
echo   web    - Compile with Emscripten and run via Node (Web/WASM)
echo   clean  - Remove build artifacts
echo.
echo Prerequisite:
echo   Ensure 'test_net.c' and 'marble_net.h' are in this directory.
goto end

:end
endlocal