@echo off
if not exist "c:\devkitPro\libctru\include\3ds.h" (
    echo ===================================================
    echo 3DS Headers are missing! Downloading them now...
    echo ===================================================
    c:\devkitPro\msys2\usr\bin\pacman.exe -S --noconfirm 3ds-dev
)

set DEVKITPRO=/c/devkitPro
set DEVKITARM=/c/devkitPro/devkitARM
set PATH=c:\devkitPro\msys2\usr\bin;c:\devkitPro\devkitARM\bin;%PATH%

echo.
echo Starting Compilation...
make > build_log.txt 2>&1
type build_log.txt
echo.
if %ERRORLEVEL% EQU 0 (
    echo BUILD SUCCESSFUL
) else (
    echo BUILD FAILED - check build_log.txt for errors
)
