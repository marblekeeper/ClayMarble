@echo off
setlocal EnableDelayedExpansion

REM === Configuration ===
set "MSYS_DIR=C:\msys64\ucrt64"
set "OUT_NAME=test_audio.exe"

REM === Setup Paths ===
set "PATH=%MSYS_DIR%\bin;C:\msys64\usr\bin;vendor\ThirdParty\bin;%PATH%"

REM === Pre-build Cleanup ===
taskkill /F /IM %OUT_NAME% >nul 2>nul

echo ================================================
echo AUDIO TEST BUILD (Minimp3 + SDL2)
echo ================================================

echo [1/2] Building...

REM UPDATED: Pointing to vendor\minimp3\minimp3 because of the git clone structure
gcc -std=c99 -O2 tests\test_audio.c -o %OUT_NAME% ^
    -Iinclude -Ivendor\minimp3\minimp3 ^
    -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -I"%MSYS_DIR%\include\SDL2" ^
    -L"%MSYS_DIR%\lib" -Lvendor\ThirdParty\bin ^
    -lmingw32 -lSDL2main -lSDL2 -lm ^
    -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] BUILD FAILED
    exit /b 1
)

echo [2/2] Running Audio Test...
if exist %OUT_NAME% (
    .\%OUT_NAME%
) else (
    echo [ERROR] executable not found.
)

exit /b 0