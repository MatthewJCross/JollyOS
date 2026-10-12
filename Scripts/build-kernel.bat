
@echo off
setlocal

echo ========================================
echo       JollyOS Kernel Build
echo ========================================
echo.

set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"

set "KERNEL_DIR=%PROJECT_DIR%\Kernel"
set "BUILD_DIR=%PROJECT_DIR%\build"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Project: %PROJECT_DIR%
echo Build:   %BUILD_DIR%
echo.

where cl.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: MSVC compiler cl.exe was not found.
    echo Run this script from the Visual Studio 2026 x64 Developer Command Prompt.
    exit /b 1
)

where ml64.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: MASM assembler ml64.exe was not found.
    echo Run this script from the Visual Studio 2026 x64 Developer Command Prompt.
    exit /b 1
)

where link.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: Microsoft linker link.exe was not found.
    exit /b 1
)

echo [1/3] Assembling entry.asm...

ml64.exe /nologo ^
    /c ^
    /Fo"%BUILD_DIR%\entry.obj" ^
    "%KERNEL_DIR%\entry.asm"

if errorlevel 1 (
    echo ERROR: Assembly failed.
    exit /b 1
)

echo.
echo [2/3] Compiling kernel.c...

cl.exe /nologo ^
    /c ^
    /TC ^
    /O2 ^
    /W4 ^
    /GS- ^
    /GR- ^
    /Zl ^
    /Oi ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /Fo"%BUILD_DIR%\kernel.obj" ^
    "%KERNEL_DIR%\kernel.c"

if errorlevel 1 (
    echo ERROR: C compilation failed.
    exit /b 1
)

if exist kernel.obj move /y kernel.obj "%BUILD_DIR%\kernel.obj" >nul

echo.
echo [3/3] Linking kernel intermediate image...

link.exe /nologo ^
    /OUT:"%BUILD_DIR%\kernel.exe" ^
    /ENTRY:_start ^
    /SUBSYSTEM:NATIVE ^
    /NODEFAULTLIB ^
    /MACHINE:X64 ^
    /BASE:0x100000 ^
    /FIXED ^
    /DYNAMICBASE:NO ^
    /INCREMENTAL:NO ^
    /OPT:REF ^
    /OPT:ICF ^
    "%BUILD_DIR%\entry.obj" ^
    "%BUILD_DIR%\kernel.obj"

if errorlevel 1 (
    echo ERROR: Linking failed.
    exit /b 1
)

echo.
echo ========================================
echo Kernel intermediate image built:
echo %BUILD_DIR%\kernel.exe
echo ========================================
echo.
echo NOTE: This is an intermediate PE image.
echo It is not yet the flat KERNEL.BIN required
echo by our planned bootloader.
echo.

exit /b 0