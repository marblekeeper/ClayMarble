@echo off
setlocal EnableDelayedExpansion

REM === Configuration ===
set MSYS_DIR=C:\msys64\ucrt64
set PATH=%MSYS_DIR%\bin;C:\msys64\usr\bin;%PATH%
set OUT_NAME=marble_phase0_2.exe

REM === MarbleEngine Phase 0.2 Build Script ===

REM 1. Pre-build Cleanup: Kill any hung processes and delete old binaries
taskkill /F /IM %OUT_NAME% >nul 2>nul
taskkill /F /IM test.exe >nul 2>nul
if exist %OUT_NAME% del /Q %OUT_NAME%
if exist test.exe del /Q test.exe

if "%1"=="test" (
    if "%2"=="msvc" (
        echo [1/2] Building tests with MSVC...
        cl /std:c11 /W4 /O2 test.c /Fe:test.exe
    ) else (
        echo [1/2] Building tests with GCC...
        REM Using -B to point directly to the bin folder for sub-tools
        gcc -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -O2 test.c -o test.exe ^
            -B "%MSYS_DIR%\bin" ^
            -L "%MSYS_DIR%\lib" ^
            -I "%MSYS_DIR%\include" ^
            -static -lm
    )
    
    REM VALIDATION: We check if the file exists instead of checking ERRORLEVEL
    if not exist test.exe (
        echo.
        echo [ERROR] test.exe was not created. 
        echo Check if Antivirus is blocking C:\msys64\ucrt64\bin\cc1.exe
        exit /b 1
    )
    
    echo.
    echo [2/2] Running tests...
    echo ----------------------------------------
    test.exe
    exit /b %ERRORLEVEL%

) else if "%1"=="msvc" (
    echo Building with MSVC...
    cl /std:c11 /W4 /O2 main.c /Fe:%OUT_NAME%

) else if "%1"=="gcc" (
    echo Building with GCC...
    
    gcc -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-variable -O2 main.c -o %OUT_NAME% ^
        -B "%MSYS_DIR%\bin" ^
        -I. -I"%MSYS_DIR%\include" ^
        -L"%MSYS_DIR%\lib" ^
        -static-libgcc -static -lm

    if not exist %OUT_NAME% (
        echo.
        echo [ERROR] %OUT_NAME% was not created.
        exit /b 1
    )

) else (
    echo Usage: build.bat [msvc^|gcc^|test]
    echo.
    echo   msvc       - Build runtime with Visual Studio cl.exe
    echo   gcc        - Build runtime with GCC/MinGW
    echo   test       - Build and run test harness (GCC)
    echo   test msvc  - Build and run test harness (MSVC)
    exit /b 1
)

echo.
echo ========================================
echo   BUILD SUCCESS: %OUT_NAME%
echo ========================================
echo Run: .\%OUT_NAME%