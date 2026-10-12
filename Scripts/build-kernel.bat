
@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"

if errorlevel 1 (
    echo ERROR: Could not initialise the Visual Studio build environment.
    pause
    exit /b 1
)

echo ========================================
echo       JollyOS Kernel Build
echo ========================================
echo.

set "PROJECT_DIR=%~dp0.."
for %%I in ("%PROJECT_DIR%") do set "PROJECT_DIR=%%~fI"

set "KERNEL_DIR=%PROJECT_DIR%\Kernel"
set "INCLUDE_DIR=%PROJECT_DIR%\Include"
set "BUILD_DIR=%PROJECT_DIR%\build"
set "SERIAL_SOURCE=%KERNEL_DIR%\serial.c"
set "SERIAL_OBJECT=%BUILD_DIR%\serial.obj"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Project: %PROJECT_DIR%
echo Build:   %BUILD_DIR%
echo Include: %INCLUDE_DIR%
echo.

if not exist "%INCLUDE_DIR%\JollyOsBootInfo.h" (
    echo ERROR: JollyOsBootInfo.h was not found.
    echo Expected: %INCLUDE_DIR%\JollyOsBootInfo.h
    exit /b 1
)

if not exist "%INCLUDE_DIR%\serial.h" (
    echo ERROR: serial.h was not found.
    echo Expected: %INCLUDE_DIR%\serial.h
    exit /b 1
)

if not exist "%SERIAL_SOURCE%" (
    echo ERROR: serial.c was not found.
    echo Expected: %SERIAL_SOURCE%
    exit /b 1
)

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

echo [1/4] Assembling entry.asm...

ml64.exe /nologo ^
    /c ^
    /Fo"%BUILD_DIR%\entry.obj" ^
    "%KERNEL_DIR%\entry.asm"

if errorlevel 1 (
    echo ERROR: Assembly failed.
    exit /b 1
)

echo.
echo [2/4] Compiling kernel.c...

cl.exe /nologo ^
    /c ^
    /TC ^
    /O2 ^
    /W4 ^
    /GS- ^
    /GR- ^
    /Zl ^
    /Oi ^
    /I"%INCLUDE_DIR%" ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /Fo"%BUILD_DIR%\kernel.obj" ^
    "%KERNEL_DIR%\kernel.c"

if errorlevel 1 (
    echo ERROR: Kernel compilation failed.
    exit /b 1
)

echo.
echo [3/4] Compiling serial.c...

cl.exe /nologo ^
    /c ^
    /TC ^
    /O2 ^
    /W4 ^
    /GS- ^
    /GR- ^
    /Zl ^
    /Oi ^
    /I"%INCLUDE_DIR%" ^
    /D_CRT_SECURE_NO_WARNINGS ^
    /Fo"%SERIAL_OBJECT%" ^
    "%SERIAL_SOURCE%"

if errorlevel 1 (
    echo ERROR: Serial compilation failed.
    exit /b 1
)

echo.
echo [4/4] Linking kernel intermediate image...

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
    "%BUILD_DIR%\kernel.obj" ^
    "%SERIAL_OBJECT%"

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
echo The current UEFI loader loads this PE image
echo directly; it is not a flat KERNEL.BIN.
echo.

pause
exit /b 0