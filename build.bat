@echo off
setlocal EnableDelayedExpansion

REM === Configuration ===
set "MSYS_DIR=C:\msys64\ucrt64"
set "EMSDK_DIR=C:\emsdk"
set "LUA_VERSION=5.4.7"
set "LUA_DIR=lua-%LUA_VERSION%"
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
if "%1"=="sprite_font_editor" goto DO_SPRITE_FONT_EDITOR
if "%1"=="web" goto DO_WEB_BUILD
if "%1"=="web_serve" goto DO_WEB_SERVE
if "%1"=="test" goto DO_TEST
if "%1"=="gcc" goto DO_GCC
if "%1"=="msvc" goto DO_MSVC

:USAGE
echo Usage: build.bat [msvc^|gcc^|test^|ui_test^|sprite_font_editor^|web^|web_serve]
echo.
echo    msvc                - Build runtime with Visual Studio cl.exe
echo    gcc                 - Build runtime with GCC/MinGW
echo    test                - Build and run test harness (GCC)
echo    test msvc           - Build and run test harness (MSVC)
echo    ui_test             - Build and run Lua UI Demo (SDL2 + EGL + Lua)
echo    sprite_font_editor  - Build and run Sprite Font Editor Tool
echo    web                 - Build for web with Emscripten
echo    web_serve           - Build for web and start local server
exit /b 1

:DO_UI_TEST
echo [1/3] Building UI Test Runtime (SDL2 + Lua + EGL)...
gcc -std=c99 -O2 test_ui.c bridge_engine.c -o %UI_OUT_NAME% ^
    -I. -I"%MSYS_DIR%\include" -I"%MSYS_DIR%\include\SDL2" ^
    -L"%MSYS_DIR%\lib" ^
    -lmingw32 -lSDL2main -lSDL2 -l%LUA_LIB% -lm ^
    -lEGL -lGLESv2 -lopengl32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi

if %ERRORLEVEL% NEQ 0 ( 
    echo [ERROR] UI BUILD FAILED 
    exit /b 1 
)

echo [2/3] Deploying Dependencies...
copy /Y "%MSYS_DIR%\bin\SDL2.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libEGL.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libGLESv2.dll" . >nul
copy /Y "%MSYS_DIR%\bin\d3dcompiler_47.dll" . >nul
copy /Y "%MSYS_DIR%\bin\zlib1.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libgcc_s_seh-1.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libstdc++-6.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libwinpthread-1.dll" . >nul
if exist "%MSYS_DIR%\bin\lua54.dll" copy /Y "%MSYS_DIR%\bin\lua54.dll" . >nul
if exist "%MSYS_DIR%\bin\lua.dll" copy /Y "%MSYS_DIR%\bin\lua.dll" . >nul

echo.
echo [3/3] Running UI Demo...
if exist %UI_OUT_NAME% (
    .\%UI_OUT_NAME%
)
exit /b 0

:DO_SPRITE_FONT_EDITOR
echo ================================================
echo SPRITE FONT EDITOR
echo ================================================
echo [1/3] Building...
gcc -std=c99 -O2 test_ui.c bridge_engine.c -o %UI_OUT_NAME% ^
    -I. -I"%MSYS_DIR%\include" -I"%MSYS_DIR%\include\SDL2" ^
    -L"%MSYS_DIR%\lib" ^
    -lmingw32 -lSDL2main -lSDL2 -l%LUA_LIB% -lm ^
    -lEGL -lGLESv2 -lopengl32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi

if %ERRORLEVEL% NEQ 0 exit /b 1

echo [2/3] Deploying Dependencies...
copy /Y "%MSYS_DIR%\bin\SDL2.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libEGL.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libGLESv2.dll" . >nul
copy /Y "%MSYS_DIR%\bin\d3dcompiler_47.dll" . >nul
copy /Y "%MSYS_DIR%\bin\zlib1.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libgcc_s_seh-1.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libstdc++-6.dll" . >nul
copy /Y "%MSYS_DIR%\bin\libwinpthread-1.dll" . >nul
if exist "%MSYS_DIR%\bin\lua54.dll" copy /Y "%MSYS_DIR%\bin\lua54.dll" . >nul
if exist "%MSYS_DIR%\bin\lua.dll" copy /Y "%MSYS_DIR%\bin\lua.dll" . >nul

echo [3/3] Launching...
if exist %UI_OUT_NAME% (
    .\%UI_OUT_NAME% sprite_font_editor
)
exit /b 0

:DO_WEB_BUILD
echo ================================================
echo BUILDING FOR WEB WITH EMSCRIPTEN
echo ================================================
echo.

REM === Find Emscripten ===
set "EMCC_PATH="
if exist "%EMSDK_DIR%\upstream\emscripten\emcc.bat" (
    set "EMCC_PATH=%EMSDK_DIR%\upstream\emscripten"
)

if "%EMCC_PATH%"=="" (
    echo [ERROR] Emscripten not found!
    exit /b 1
)

set "PATH=%EMCC_PATH%;%EMSDK_DIR%;%PATH%"

REM === Download and extract Lua if needed ===
echo [1/3] Preparing Lua %LUA_VERSION% for WebAssembly...

if not exist "%LUA_DIR%\src\lua.h" (
    if not exist "lua-%LUA_VERSION%.tar.gz" (
        echo Downloading Lua...
        powershell -Command "Invoke-WebRequest -Uri 'https://www.lua.org/ftp/lua-%LUA_VERSION%.tar.gz' -OutFile 'lua-%LUA_VERSION%.tar.gz'"
        if !ERRORLEVEL! NEQ 0 (
            echo [ERROR] Download failed!
            exit /b 1
        )
    )
    
    echo Extracting Lua...
    tar -xzf lua-%LUA_VERSION%.tar.gz
    if !ERRORLEVEL! NEQ 0 (
        echo [ERROR] Extraction failed!
        exit /b 1
    )
)

if not exist "%LUA_DIR%\src\lua.h" (
    echo [ERROR] Lua sources not found!
    exit /b 1
)

echo Lua sources ready.
echo.

REM === Compile Lua for WebAssembly ===
echo [2/3] Compiling Lua for WebAssembly...

pushd %LUA_DIR%\src

REM Compile all Lua source files to WebAssembly object files
call emcc -c -O2 -DLUA_USE_POSIX ^
    lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c lmem.c ^
    lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c lundump.c ^
    lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c lmathlib.c ^
    loadlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Lua compilation failed!
    popd
    exit /b 1
)

REM Create static library
call emar rcs liblua_wasm.a *.o

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to create Lua archive!
    popd
    exit /b 1
)

popd
echo Lua compiled for WebAssembly.
echo.

REM === Build WebAssembly Application ===
echo [3/3] Building WebAssembly application...
if not exist "web" mkdir web

call emcc test_ui.c bridge_engine.c -o web/index.html ^
    -I. -I%LUA_DIR%\src ^
    %LUA_DIR%\src\liblua_wasm.a ^
    -s USE_SDL=2 ^
    -s FULL_ES2=1 ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s INITIAL_MEMORY=67108864 ^
    -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" ^
    -s EXPORTED_FUNCTIONS="['_main']" ^
    --preload-file framework.lua ^
    --preload-file sprite_font_editor.lua ^
    --preload-file demo.lua ^
    --preload-file demo_complete.lua ^
    --preload-file Content@/Content ^
    -O2 ^
    -std=c99 ^
    --shell-file shell_minimal.html

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Web build failed!
    exit /b 1
)

echo.
echo ================================================
echo BUILD SUCCESS!
echo ================================================
echo.
echo Files in web/:
echo   - index.html
echo   - index.js
echo   - index.wasm
echo   - index.data
echo.
echo To run:
echo   python -m http.server 8000
echo   Then: http://localhost:8000/web/
echo ================================================
exit /b 0

:DO_WEB_SERVE
call :DO_WEB_BUILD
if %ERRORLEVEL% NEQ 0 exit /b 1
echo.
echo [Starting server...]
echo Open: http://localhost:8000/web/
python -m http.server 8000
exit /b 0

:DO_TEST
if "%2"=="msvc" (
    cl /std:c11 /W4 /O2 test.c /Fe:test.exe /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 test_cmd.c /Fe:test_cmd.exe /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 test_items.c /Fe:test_items.exe /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 test_gen.c /Fe:test_gen.exe /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
) else (
    gcc -std=c99 -w -O2 test.c -o test.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 test_cmd.c -o test_cmd.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 test_items.c -o test_items.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 test_gen.c -o test_gen.exe -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
)
if %ERRORLEVEL% NEQ 0 exit /b 1
if exist test.exe .\test.exe
if exist test_cmd.exe .\test_cmd.exe
if exist test_items.exe .\test_items.exe
if exist test_gen.exe .\test_gen.exe
exit /b 0

:DO_GCC
gcc -std=c99 -w -O2 main.c -o %OUT_NAME% -I. -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
goto FINISH

:DO_MSVC
cl /std:c11 /W4 /O2 main.c /Fe:%OUT_NAME% /I. /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
goto FINISH

:FINISH
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build Success: %OUT_NAME%
) else (
    echo.
    echo [ERROR] BUILD FAILED
)
exit /b %ERRORLEVEL%