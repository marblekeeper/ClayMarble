@echo off
setlocal EnableDelayedExpansion

echo [Senior Refactor] Initializing directory structure...

REM 1. Create Folder Hierarchy
if not exist "src" mkdir "src"
if not exist "include" mkdir "include"
if not exist "scripts\core" mkdir "scripts\core"
if not exist "scripts\demos" mkdir "scripts\demos"
if not exist "assets" mkdir "assets"
if not exist "tests" mkdir "tests"
if not exist "bin" mkdir "bin"
if not exist "vendor" mkdir "vendor"
if not exist "tools" mkdir "tools"

echo [Moving] C Source and Headers...
move /Y bridge_engine.c src\
move /Y input_handler.c src\
move /Y main.c src\
move /Y *.h include\

echo [Moving] Tests...
move /Y test.c tests\
move /Y test_cmd.c tests\
move /Y test_gen.c tests\
move /Y test_items.c tests\
move /Y test_ui.c tests\

echo [Moving] Lua Scripts...
move /Y framework.lua scripts\core\
move /Y marble_compile.lua scripts\core\
move /Y demo.lua scripts\demos\
move /Y demo_complete.lua scripts\demos\
move /Y space_shooter.lua scripts\demos\
move /Y dungeon_crawl.lua scripts\demos\
move /Y sprite_font_editor.lua scripts\demos\
move /Y MindMarr.lua scripts\demos\

echo [Moving] Assets & Content...
move /Y oak_forest.marble assets\
if exist "Content" move /Y Content assets\

echo [Moving] Third Party & Vendor...
if exist "lua-5.4.7" move /Y lua-5.4.7 vendor\
if exist "lua_headers" move /Y lua_headers vendor\
if exist "ThirdParty" move /Y ThirdParty vendor\

echo [Moving] Tools & Cleanup...
move /Y cleanup.bat tools\

echo [Cleanup] Removing empty Backups...
if exist "Backups" rmdir /S /Q Backups

echo [Done] Root is now clean. Run the new build.bat to compile.
pause