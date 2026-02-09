@echo off
setlocal EnableDelayedExpansion

REM === Configuration ===
set "MSYS_DIR=C:\msys64\ucrt64"
set "EMSDK_DIR=C:\emsdk"
set "LUA_VERSION=5.4.7"
set "LUA_DIR=vendor\lua-%LUA_VERSION%"
set "PATH=%MSYS_DIR%\bin;C:\msys64\usr\bin;vendor\ThirdParty\bin;%PATH%"
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
if "%1"=="mindmarr" goto DO_MINDMARR
if "%1"=="web" goto DO_WEB_BUILD
if "%1"=="web_serve" goto DO_WEB_SERVE
if "%1"=="test" goto DO_TEST
if "%1"=="gcc" goto DO_GCC
if "%1"=="msvc" goto DO_MSVC
if "%1"=="clean_dlls" goto DO_CLEAN_DLLS

:USAGE
echo Usage: build.bat [msvc^|gcc^|test^|ui_test^|sprite_font_editor^|mindmarr^|web^|web_serve^|clean_dlls]
echo.
echo    msvc                - Build runtime with Visual Studio cl.exe
echo    gcc                 - Build runtime with GCC/MinGW
echo    test                - Build and run test harness (GCC)
echo    test msvc           - Build and run test harness (MSVC)
echo    ui_test             - Build and run Lua UI Demo (SDL2 + EGL + Lua)
echo    sprite_font_editor  - Build and run Sprite Font Editor Tool
echo    mindmarr            - Build and run MindMarr game
echo    web                 - Build for web with Emscripten
echo    web_serve           - Build for web and start local server
echo    clean_dlls          - Remove all DLLs from project root
exit /b 1

:DO_CLEAN_DLLS
echo Cleaning DLLs from project root...
del /Q SDL2.dll 2>nul
del /Q libEGL.dll 2>nul
del /Q libGLESv2.dll 2>nul
del /Q d3dcompiler_47.dll 2>nul
del /Q zlib1.dll 2>nul
del /Q libgcc_s_seh-1.dll 2>nul
del /Q libstdc++-6.dll 2>nul
del /Q libwinpthread-1.dll 2>nul
del /Q lua54.dll 2>nul
del /Q lua.dll 2>nul
echo Done.
exit /b 0

:DO_UI_TEST
echo [1/2] Building UI Test Runtime (SDL2 + Lua + EGL)...
gcc -std=c99 -O2 tests\test_ui.c src\bridge_engine.c src\input_handler.c -o %UI_OUT_NAME% ^
    -Iinclude -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -I"%MSYS_DIR%\include\SDL2" ^
    -L"%MSYS_DIR%\lib" -Lvendor\ThirdParty\bin ^
    -lmingw32 -lSDL2main -lSDL2 -l%LUA_LIB% -lm ^
    -lEGL -lGLESv2 -lopengl32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi

if %ERRORLEVEL% NEQ 0 ( 
    echo [ERROR] UI BUILD FAILED 
    exit /b 1 
)

echo.
echo [2/2] Running UI Demo...
if exist %UI_OUT_NAME% (
    .\%UI_OUT_NAME%
)
exit /b 0

:DO_SPRITE_FONT_EDITOR
echo ================================================
echo SPRITE FONT EDITOR
echo ================================================
echo [1/2] Building...
gcc -std=c99 -O2 tests\test_ui.c src\bridge_engine.c src\input_handler.c -o %UI_OUT_NAME% ^
    -Iinclude -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -I"%MSYS_DIR%\include\SDL2" ^
    -L"%MSYS_DIR%\lib" -Lvendor\ThirdParty\bin ^
    -lmingw32 -lSDL2main -lSDL2 -l%LUA_LIB% -lm ^
    -lEGL -lGLESv2 -lopengl32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi

if %ERRORLEVEL% NEQ 0 exit /b 1

echo [2/2] Launching...
if exist %UI_OUT_NAME% (
    .\%UI_OUT_NAME% sprite_font_editor
)
exit /b 0

:DO_MINDMARR
echo ================================================
echo MINDMARR
echo ================================================
echo [1/2] Building...
gcc -std=c99 -O2 tests\test_ui.c src\bridge_engine.c src\input_handler.c -o %UI_OUT_NAME% ^
    -Iinclude -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -I"%MSYS_DIR%\include\SDL2" ^
    -L"%MSYS_DIR%\lib" -Lvendor\ThirdParty\bin ^
    -lmingw32 -lSDL2main -lSDL2 -l%LUA_LIB% -lm ^
    -lEGL -lGLESv2 -lopengl32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lversion -luuid -lsetupapi

if %ERRORLEVEL% NEQ 0 exit /b 1

echo [2/2] Launching MindMarr...
if exist %UI_OUT_NAME% (
    .\%UI_OUT_NAME% MindMarr
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

REM === Check Lua sources ===
echo [1/3] Checking Lua %LUA_VERSION% for WebAssembly...

if not exist "%LUA_DIR%\src\lua.h" (
    echo [ERROR] Lua sources not found at %LUA_DIR%\src\
    echo Expected location: vendor\lua-5.4.7\src\
    exit /b 1
)

echo Lua sources found.
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

echo [DEBUG] Current directory: %CD%
echo [DEBUG] Checking if scripts folder exists...
if exist "scripts" (
    echo [DEBUG] scripts folder found
) else (
    echo [ERROR] scripts folder NOT found!
)

call emcc tests\test_ui.c src\bridge_engine.c src\input_handler.c -o web\index.html ^
    -Iinclude -Ivendor\ThirdParty\include -I%LUA_DIR%\src ^
    %LUA_DIR%\src\liblua_wasm.a ^
    -s USE_SDL=2 ^
    -s FULL_ES2=1 ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s INITIAL_MEMORY=67108864 ^
    -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap']" ^
    -s EXPORTED_FUNCTIONS="['_main']" ^
    --preload-file scripts@/scripts ^
    --preload-file "MindMarr@/MindMarr" ^
    --preload-file assets@/assets ^
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
    cl /std:c11 /W4 /O2 tests\test.c /Fe:test.exe /Iinclude /Ivendor\ThirdParty\include /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 tests\test_cmd.c /Fe:test_cmd.exe /Iinclude /Ivendor\ThirdParty\include /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 tests\test_items.c /Fe:test_items.exe /Iinclude /Ivendor\ThirdParty\include /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
    cl /std:c11 /W4 /O2 tests\test_gen.c /Fe:test_gen.exe /Iinclude /Ivendor\ThirdParty\include /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
) else (
    gcc -std=c99 -w -O2 tests\test.c -o test.exe -Iinclude -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 tests\test_cmd.c -o test_cmd.exe -Iinclude -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 tests\test_items.c -o test_items.exe -Iinclude -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
    gcc -std=c99 -w -O2 tests\test_gen.c -o test_gen.exe -Iinclude -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
)
if %ERRORLEVEL% NEQ 0 exit /b 1
if exist test.exe .\test.exe
if exist test_cmd.exe .\test_cmd.exe
if exist test_items.exe .\test_items.exe
if exist test_gen.exe .\test_gen.exe
exit /b 0

:DO_GCC
gcc -std=c99 -w -O2 src\main.c -o %OUT_NAME% -Iinclude -Ivendor\ThirdParty\include -I"%MSYS_DIR%\include" -L"%MSYS_DIR%\lib" -static -l%LUA_LIB% -lm
goto FINISH

:DO_MSVC
cl /std:c11 /W4 /O2 src\main.c /Fe:%OUT_NAME% /Iinclude /Ivendor\ThirdParty\include /I"%MSYS_DIR%\include" /link /LIBPATH:"%MSYS_DIR%\lib" %LUA_LIB%.lib
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