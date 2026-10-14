@echo off
setlocal EnableExtensions

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
set "VIRTUAL_MEMORY_SOURCE=%KERNEL_DIR%\VirtualMemory.c"
set "VIRTUAL_MEMORY_OBJECT=%BUILD_DIR%\VirtualMemory.obj"
set "KERNEL_HEAP_SOURCE=%KERNEL_DIR%\KernelHeap.c"
set "KERNEL_HEAP_OBJECT=%BUILD_DIR%\KernelHeap.obj"
set "PORT_IO_SOURCE=%KERNEL_DIR%\PortIo.asm"
set "PORT_IO_OBJECT=%BUILD_DIR%\PortIo.obj"
set "KEYBOARD_SOURCE=%KERNEL_DIR%\Keyboard.c"
set "KEYBOARD_OBJECT=%BUILD_DIR%\Keyboard.obj"

where cl.exe >nul 2>&1

if errorlevel 1 call :InitialiseVisualStudio

if errorlevel 1 exit /b 1

goto :VisualStudioReady

:InitialiseVisualStudio
echo Initialising Visual Studio 2026 x64...
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if errorlevel 1 (
    echo ERROR: Could not initialise the Visual Studio build environment.
    exit /b 1
)

where cl.exe >nul 2>&1

if errorlevel 1 (
    echo ERROR: MSVC compiler cl.exe was not found.
    exit /b 1
)

exit /b 0

:VisualStudioReady

echo.

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

if not exist "%INCLUDE_DIR%\VirtualMemory.h" (
    echo ERROR: VirtualMemory.h was not found.
    echo Expected: %INCLUDE_DIR%\VirtualMemory.h
    exit /b 1
)

if not exist "%VIRTUAL_MEMORY_SOURCE%" (
    echo ERROR: VirtualMemory.c was not found.
    echo Expected: %VIRTUAL_MEMORY_SOURCE%
    exit /b 1
)

if not exist "%INCLUDE_DIR%\KernelHeap.h" (
    echo ERROR: KernelHeap.h was not found.
    echo Expected: %INCLUDE_DIR%\KernelHeap.h
    exit /b 1
)

if not exist "%KERNEL_HEAP_SOURCE%" (
    echo ERROR: KernelHeap.c was not found.
    echo Expected: %KERNEL_HEAP_SOURCE%
    exit /b 1
)


if not exist "%INCLUDE_DIR%\PortIo.h" (
    echo ERROR: PortIo.h was not found.
    echo Expected: %INCLUDE_DIR%\PortIo.h
    exit /b 1
)

if not exist "%PORT_IO_SOURCE%" (
    echo ERROR: PortIo.asm was not found.
    echo Expected: %PORT_IO_SOURCE%
    exit /b 1
)

if not exist "%INCLUDE_DIR%\Keyboard.h" (
    echo ERROR: Keyboard.h was not found.
    echo Expected: %INCLUDE_DIR%\Keyboard.h
    exit /b 1
)

if not exist "%KEYBOARD_SOURCE%" (
    echo ERROR: Keyboard.c was not found.
    echo Expected: %KEYBOARD_SOURCE%
    exit /b 1
)

where ml64.exe >nul 2>&1

if errorlevel 1 (
    echo ERROR: MASM assembler ml64.exe was not found.
    exit /b 1
)

where link.exe >nul 2>&1

if errorlevel 1 (
    echo ERROR: Microsoft linker link.exe was not found.
    exit /b 1
)

echo [1/8] Assembling entry.asm...

ml64.exe /nologo ^
    /c ^
    /Fo"%BUILD_DIR%\entry.obj" ^
    "%KERNEL_DIR%\entry.asm"

if errorlevel 1 (
    echo ERROR: Assembly failed.
    exit /b 1
)

echo.
echo [2/8] Assembling PortIo.asm...
ml64.exe /nologo ^
    /c ^
    /Fo"%PORT_IO_OBJECT%" ^
    "%PORT_IO_SOURCE%"

if errorlevel 1 (
    echo ERROR: Failed to assemble PortIo.asm.
    exit /b 1
)

echo.
echo [3/8] Compiling kernel.c...

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
echo [4/8] Compiling serial.c...

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
echo [5/8] Compiling VirtualMemory.c...

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
    /Fo"%VIRTUAL_MEMORY_OBJECT%" ^
    "%VIRTUAL_MEMORY_SOURCE%"

if errorlevel 1 (
    echo ERROR: Virtual memory compilation failed.
    exit /b 1
)

echo.
echo [6/8] Compiling KernelHeap.c...

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
    /Fo"%KERNEL_HEAP_OBJECT%" ^
    "%KERNEL_HEAP_SOURCE%"

if errorlevel 1 (
    echo ERROR: Kernel heap compilation failed.
    exit /b 1
)

echo.
echo [7/8] Compiling Keyboard.c...

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
    /Fo"%KEYBOARD_OBJECT%" ^
    "%KEYBOARD_SOURCE%"

if errorlevel 1 (
    echo ERROR: Keyboard compilation failed.
    exit /b 1
)

echo.
echo [8/8] Linking kernel intermediate image...

echo.
echo DEBUG: Linker inputs
echo Entry:       "%BUILD_DIR%\entry.obj"
echo Kernel:      "%BUILD_DIR%\kernel.obj"
echo Serial:      "%SERIAL_OBJECT%"
echo VirtualMemory: "%VIRTUAL_MEMORY_OBJECT%"
echo.
echo.
echo DEBUG: Linker executable
where link.exe
echo.

@echo on
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
    "%SERIAL_OBJECT%" ^
    "%VIRTUAL_MEMORY_OBJECT%" ^
    "%BUILD_DIR%\KernelHeap.obj" ^
    "%PORT_IO_OBJECT%" ^
    "%KEYBOARD_OBJECT%"

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

exit /b 0