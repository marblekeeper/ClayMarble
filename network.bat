@echo off
setlocal EnableDelayedExpansion

REM =========================================================================
REM network.bat -- MarbleEngine Network Protocol Build & Test
REM
REM Uses same toolchain config as build.bat
REM
REM Usage:
REM   network.bat win        Build + run unit tests
REM   network.bat win demo   Build + run interactive WASD demo
REM   network.bat web        Build for WASM + run via Node
REM   network.bat lua        Run Lua self-tests
REM   network.bat all        Run all tests
REM =========================================================================

REM === Configuration (matches build.bat) ===
set "MSYS_DIR=C:\msys64\ucrt64"
set "EMSDK_DIR=C:\emsdk"
set "PATH=%MSYS_DIR%\bin;C:\msys64\usr\bin;%PATH%"

if "%1"=="" goto :usage
if /i "%1"=="win" goto :do_win
if /i "%1"=="web" goto :do_web
if /i "%1"=="lua" goto :do_lua
if /i "%1"=="all" goto :do_all
goto :usage

REM =========================================================================
:do_win
echo ============================================
echo  MarbleEngine Network Protocol - Windows
echo ============================================
echo.
taskkill /F /IM test_net.exe >nul 2>nul
echo [BUILD] gcc -std=c99 -w -O2 test_net.c -o test_net.exe
gcc -std=c99 -w -O2 test_net.c -o test_net.exe -Iinclude -I"%MSYS_DIR%\include" -static -lm
if %ERRORLEVEL% neq 0 goto :win_fail
echo [BUILD] OK
echo.
if /i "%2"=="demo" goto :win_demo
test_net.exe
exit /b %ERRORLEVEL%
:win_demo
test_net.exe --demo
exit /b %ERRORLEVEL%
:win_fail
echo.
echo [ERROR] Compilation failed.
exit /b 1

REM =========================================================================
:do_web
echo ============================================
echo  MarbleEngine Network Protocol - Web/WASM
echo ============================================
echo.

set "EMCC_PATH="
if exist "%EMSDK_DIR%\upstream\emscripten\emcc.bat" (
    set "EMCC_PATH=%EMSDK_DIR%\upstream\emscripten"
)
if "!EMCC_PATH!"=="" (
    echo [ERROR] Emscripten not found at %EMSDK_DIR%
    exit /b 1
)
set "PATH=!EMCC_PATH!;%EMSDK_DIR%;%PATH%"

echo [BUILD] emcc -std=c99 -O2 test_net.c -o test_net.js
call emcc -std=c99 -O2 test_net.c -o test_net.js -sENVIRONMENT=node
if %ERRORLEVEL% neq 0 goto :web_fail
echo [BUILD] OK
echo.
where node >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [WARN] Node.js not found. Built WASM but can't run.
    exit /b 0
)
node test_net.js
exit /b %ERRORLEVEL%
:web_fail
echo.
echo [ERROR] emcc compilation failed.
exit /b 1

REM =========================================================================
:do_lua
echo ============================================
echo  MarbleEngine Network Protocol - Lua Tests
echo ============================================
echo.

REM Try each Lua interpreter
for %%L in (lua lua54 lua5.4 lua5.3 luajit) do (
    where %%L >nul 2>nul
    if !ERRORLEVEL! equ 0 (
        echo [RUN] Using %%L
        echo.
        %%L -e "local net = dofile('network.lua'); os.exit(net.selfTest() and 0 or 1)"
        exit /b !ERRORLEVEL!
    )
)
echo [ERROR] No Lua interpreter found.
exit /b 1

REM =========================================================================
:do_all
echo ============================================
echo  MarbleEngine Network Protocol - All Tests
echo ============================================
echo.

REM --- C Tests ---
echo [1/2] C Unit Tests
echo -------------------
taskkill /F /IM test_net.exe >nul 2>nul
echo gcc -std=c99 -w -O2 test_net.c -o test_net.exe
gcc -std=c99 -w -O2 test_net.c -o test_net.exe -Iinclude -I"%MSYS_DIR%\include" -static -lm
if %ERRORLEVEL% neq 0 (
    echo [ERROR] GCC compilation failed.
    set C_OK=no
    goto :all_lua
)
test_net.exe
if %ERRORLEVEL% neq 0 (
    set C_OK=no
    goto :all_lua
)
set C_OK=yes

:all_lua
echo.
echo [2/2] Lua Self-Tests
echo ---------------------
set LUA_OK=skip
for %%L in (lua lua54 lua5.4 lua5.3 luajit) do (
    if "!LUA_OK!"=="skip" (
        where %%L >nul 2>nul
        if !ERRORLEVEL! equ 0 (
            %%L -e "local net = dofile('network.lua'); os.exit(net.selfTest() and 0 or 1)"
            if !ERRORLEVEL! equ 0 (set LUA_OK=yes) else (set LUA_OK=no)
        )
    )
)
if "!LUA_OK!"=="skip" echo [SKIP] No Lua interpreter found.

echo.
echo ============================================
echo  SUMMARY
echo ============================================
if "!C_OK!"=="yes" (echo   C:   PASSED) else (echo   C:   FAILED)
if "!LUA_OK!"=="yes" (echo   Lua: PASSED) else if "!LUA_OK!"=="skip" (echo   Lua: SKIPPED) else (echo   Lua: FAILED)
echo ============================================

if "!C_OK!"=="yes" if "!LUA_OK!"=="yes" exit /b 0
if "!C_OK!"=="yes" if "!LUA_OK!"=="skip" exit /b 0
exit /b 1

REM =========================================================================
:usage
echo.
echo Usage: network.bat [win/web/lua/all] [demo]
echo.
echo   win        Build + run C unit tests (GCC)
echo   win demo   Build + run interactive WASD demo
echo   web        Build + run C unit tests (Emscripten/WASM)
echo   lua        Run Lua self-tests
echo   all        Run all tests (C + Lua)
exit /b 1