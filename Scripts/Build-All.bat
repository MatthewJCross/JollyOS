@echo off
setlocal

echo ==========================================
echo           JollyOS Build All
echo ==========================================
echo.

set "PROJECT_DIR=F:\JollyOs"
set "EDK2_DIR=C:\edk2"
set "VS_ROOT=C:\Program Files\Microsoft Visual Studio\18\Professional"
set "VS_VERSION=14.52.36725"
set "VS2026_PREFIX=%VS_ROOT%\VC\Tools\MSVC\%VS_VERSION%\"
set "BOOTLOADER_OUTPUT=%EDK2_DIR%\Build\JollyOS\DEBUG_VS2026\X64\Bootloader\Bootloader\OUTPUT\Bootloader.efi"
set "BOOTLOADER_DEST=%PROJECT_DIR%\build\EFI\BOOT\BOOTX64.EFI"

if not exist "%PROJECT_DIR%\JollyOS.dsc" (
    echo ERROR: JollyOS.dsc was not found.
    echo Expected: %PROJECT_DIR%\JollyOS.dsc
    exit /b 1
)

if not exist "%EDK2_DIR%\edksetup.bat" (
    echo ERROR: EDK II was not found at %EDK2_DIR%.
    exit /b 1
)

if not exist "%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat" (
    echo ERROR: Visual Studio 2026 x64 environment script was not found.
    exit /b 1
)

if not exist "%VS2026_PREFIX%bin\Hostx64\x64\cl.exe" (
    echo ERROR: The expected MSVC compiler was not found.
    echo Check the VS_VERSION setting in this script.
    exit /b 1
)

if not exist "C:\Program Files\NASM\nasm.exe" (
    echo ERROR: NASM was not found.
    echo Expected: C:\Program Files\NASM\nasm.exe
    exit /b 1
)

echo [1/5] Initialising Visual Studio 2026 x64...

call "%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat"

if errorlevel 1 (
    echo ERROR: Visual Studio environment initialisation failed.
    exit /b 1
)

set "VS2026_PREFIX=%VS_ROOT%\VC\Tools\MSVC\%VS_VERSION%\"
set "NASM_PREFIX=C:\Program Files\NASM\"
set "PYTHON_COMMAND=py -3.13"
set "PACKAGES_PATH=%PROJECT_DIR%;%EDK2_DIR%"

echo.
echo [2/5] Initialising EDK II...

cd /d "%EDK2_DIR%"

call "%EDK2_DIR%\edksetup.bat" VS2026

if errorlevel 1 (
    echo ERROR: EDK II environment initialisation failed.
    exit /b 1
)

echo.
echo [3/5] Building UEFI bootloader...

build -p "%PROJECT_DIR%\JollyOS.dsc" -a X64 -t VS2026 -b DEBUG

if errorlevel 1 (
    echo ERROR: UEFI bootloader build failed.
    exit /b 1
)

if not exist "%BOOTLOADER_OUTPUT%" (
    echo ERROR: Built Bootloader.efi was not found.
    echo Expected: %BOOTLOADER_OUTPUT%
    exit /b 1
)

echo.
echo [4/5] Building kernel...

call "%PROJECT_DIR%\Scripts\build-kernel.bat"

if errorlevel 1 (
    echo ERROR: Kernel build failed.
    exit /b 1
)

if not exist "%PROJECT_DIR%\build\kernel.exe" (
    echo ERROR: kernel.exe was not created.
    exit /b 1
)

echo.
echo [5/5] Preparing boot files...

if not exist "%PROJECT_DIR%\build\EFI\BOOT" (
    mkdir "%PROJECT_DIR%\build\EFI\BOOT"
)

if errorlevel 1 (
    echo ERROR: Could not create the EFI boot directory.
    exit /b 1
)

copy /Y "%BOOTLOADER_OUTPUT%" "%BOOTLOADER_DEST%" >nul

if errorlevel 1 (
    echo ERROR: Could not copy Bootloader.efi.
    exit /b 1
)

echo.
echo ==========================================
echo          JollyOS BUILD SUCCEEDED
echo ==========================================
echo.
echo Bootloader:
echo %BOOTLOADER_DEST%
echo.
echo Kernel:
echo %PROJECT_DIR%\build\kernel.exe
echo.
echo NOTE: The VMware VHD/VMDK has NOT been updated.
echo.
echo Press any key to close this window...
pause >nul
exit /b 0

