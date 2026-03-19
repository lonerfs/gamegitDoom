@echo off
echo Doom Map Creator
echo ================

if "%1"=="" (
    echo Usage: create_map.bat mapname.txt
    exit /b 1
)

set MAPNAME=%1
set WADNAME=mymaps.wad
set MAPID=MAP01

echo Creating map %MAPNAME% as %MAPID% in %WADNAME%
map_converter --create %MAPNAME% %WADNAME% %MAPID%

if %errorlevel% equ 0 (
    echo Success! You can now run game.exe
) else (
    echo Failed to create map
)