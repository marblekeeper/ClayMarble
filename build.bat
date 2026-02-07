@echo off
setlocal EnableDelayedExpansion

REM === Configuration ===
set "MSYS_DIR=C:\msys64\ucrt64"
set "PATH=%MSYS_DIR%\bin;C:\msys64\usr\bin;%PATH%"
set "OUT_NAME=marble_phase0_2.exe"
set "UI_OUT_NAME=marble_ui.exe"

REM === Detect Lua Library Name ===
set "LUA_LIB=lua"
if exist "%MSYS_DIR%\lib\liblua54.a" set "LUA_LIB=lua54"

REM === Pre-build Cleanup ===
taskkill /F /IM %OUT_NAME% >nul 2>nul
taskkill /F /IM %UI_OUT_NAME% >nul 2>nul
taskkill /F /IM test.exe >nul 2>nul
taskkill /F /IM test_cmd.exe >nul 2>nul
taskkill /F /IM test_items.exe >nul 2>nul
taskkill /F /IM test_gen.exe >nul 2>nul

REM === Logic Branching ===
if "%1"=="ui_test" goto DO_UI_TEST
if "%1"=="test" goto DO_TEST
if "%1"=="gcc" goto DO_GCC
if "%1"=="msvc" goto DO_MSVC

:USAGE
echo Usage: build.bat [msvc^|gcc^|test^|ui_test]
echo.
echo    msvc       - Build runtime with Visual Studio cl.exe
echo    gcc        - Build runtime with GCC/MinGW
echo    test       - Build and run test harness (GCC)
echo    test msvc  - Build and run test harness (MSVC)
echo    ui_test    - Build and run Lua UI Demo (SDL2 + EGL + Lua)
exit /b 1

:DO_UI_TEST
echo [1/3] Building UI Test Runtime (SDL2 + Lua + EGL)...
REM Compiles the C Host (test_ui.c) and the Graphics Backend (bridge_engine.c) together
REM Links: SDL2 (Window/Input), Lua (Scripting), EGL/GLESv2 (Rendering)
gcc -std=c99 -O2 test_ui.c bridge_engine.c -o %UI_OUT_NAME% ^
    -I. -I"%MSYS_DIR%\include" -I"%MSYS_DIR%\include\SDL2" ^
    -L"%MSYS_DIR%\lib" ^
    -lmingw32 -lSDL2main -lSDL2 -l%LUA_LIB% -lm ^
    -lEGL -lGLESv2 -lopengl32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi

if %ERRORLEVEL% NEQ 0 ( 
    echo [ERROR] UI BUILD FAILED 
    echo Ensure SDL2, Lua, and ANGLE/EGL packages are installed in MSYS2.
    exit /b 1 
)

echo [2/3] Deploying Dependencies...
REM Copy necessary DLLs to local folder so the EXE can run
copy /Y "%MSYS_DIR%\bin\SDL2.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libEGL.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libGLESv2.dll" . >nul
copy /Y "%MSYS_DIR%\bin\d3dcompiler_47.dll" . >nul
copy /Y "%MSYS_DIR%\bin\zlib1.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libgcc_s_seh-1.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libstdc++-6.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libwinpthread-1.dll" . >nul

REM Copy Lua DLL (name varies slightly by distro)
if exist "%MSYS_DIR%\bin\lua54.dll" copy /Y "%MSYS_DIR%\bin\lua54.dll" . >nul
if exist "%MSYS_DIR%\bin\lua.dll" copy /Y "%MSYS_DIR%\bin\lua.dll" . >nul

echo.
echo [3/3] Running UI Demo...
echo ----------------------------------------
if exist %UI_OUT_NAME% (
    .\%UI_OUT_NAME%
)
exit /b 0

:DO_TEST
if "%2"=="msvc" (
    echo [1/3] Building tests with MSVC...
    cl /std:c11 /W4 /O2 test.c /Fe:test.exe /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 test_cmd.c /Fe:test_cmd.exe /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 test_items.c /Fe:test_items.exe /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 test_gen.c /Fe:test_gen.exe /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
) else (
    echo [1/3] Building tests with GCC...
    gcc -std=c99 -w -O2 test.c -o test.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 test_cmd.c -o test_cmd.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 test_items.c -o test_items.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 test_gen.c -o test_gen.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
)

if %ERRORLEVEL% NEQ 0 ( echo [ERROR] TEST BUILD FAILED & exit /b 1 )

echo.
echo [2/3] Running Test Suite...
echo ----------------------------------------
if exist test.exe (
    echo Running core tests...
    .\test.exe
    if !ERRORLEVEL! NEQ 0 exit /b 1
)
if exist test_cmd.exe (
    echo Running command buffer tests...
    .\test_cmd.exe
    if !ERRORLEVEL! NEQ 0 exit /b 1
)
if exist test_items.exe (
    echo Running item tests...
    .\test_items.exe
    if !ERRORLEVEL! NEQ 0 exit /b 1
)
if exist test_gen.exe (
    echo Running generated code tests...
    .\test_gen.exe
    if !ERRORLEVEL! NEQ 0 exit /b 1
)
exit /b 0

:DO_GCC
echo Building with GCC...
gcc -std=c99 -w -O2 main.c -o %OUT_NAME% -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
goto FINISH

:DO_MSVC
echo Building with MSVC...
cl /std:c11 /W4 /O2 main.c /Fe:%OUT_NAME% /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
goto FINISH

:FINISH
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build Success: %OUT_NAME%
    echo Run with: %OUT_NAME%
) else (
    echo.
    echo [ERROR] BUILD FAILED
)
exit /b %ERRORLEVEL%