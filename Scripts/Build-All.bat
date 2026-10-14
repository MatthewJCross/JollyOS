cls
@echo off
setlocal EnableExtensions EnableDelayedExpansion

echo ==========================================
echo           JollyOS Build All
echo ==========================================
echo.

set "PROJECT_DIR=F:\JollyOS"
set "EDK2_DIR=C:\edk2"
set "VS_ROOT=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
set "VS_ENV=!VS_ROOT!\VC\Auxiliary\Build\vcvars64.bat"
set "NASM_PREFIX=C:\Program Files\NASM/"
set "BOOTLOADER_OUTPUT=!EDK2_DIR!\Build\JollyOS\DEBUG_VS2026\X64\Bootloader\Bootloader\OUTPUT\Bootloader.efi"
set "BOOTLOADER_DEST=!PROJECT_DIR!\build\EFI\BOOT\BOOTX64.EFI"
set "KERNEL_OUTPUT=!PROJECT_DIR!\build\kernel.exe"

if not exist "!PROJECT_DIR!\JollyOS.dsc" (
    echo ERROR: JollyOS.dsc was not found.
    goto :fail
)

if not exist "!EDK2_DIR!\edksetup.bat" (
    echo ERROR: EDK II was not found.
    goto :fail
)

if not exist "!VS_ENV!" (
    echo ERROR: Visual Studio environment script was not found.
    goto :fail
)

if not exist "!NASM_PREFIX!\nasm.exe" (
    echo ERROR: NASM was not found.
    goto :fail
)

echo [1/5] Initialising Visual Studio 2026 x64...
echo.

call "!VS_ENV!"

if errorlevel 1 (
    echo ERROR: Visual Studio initialisation failed.
    goto :fail
)

where cl.exe >nul 2>&1

if errorlevel 1 (
    echo ERROR: MSVC compiler was not found.
    goto :fail
)

echo MSVC compiler:
where cl.exe

echo.
echo [2/5] Initialising EDK II...
echo.

cd /d "!EDK2_DIR!"

if errorlevel 1 (
    echo ERROR: Could not change to the EDK II directory.
    goto :fail
)

set "PYTHON_COMMAND=py -3.13"
set "PACKAGES_PATH=!PROJECT_DIR!;!EDK2_DIR!"

call "!EDK2_DIR!\edksetup.bat" VS2026

if errorlevel 1 (
    echo ERROR: EDK II environment initialisation failed.
    goto :fail
)

where build >nul 2>&1

if errorlevel 1 (
    echo ERROR: EDK II build command was not found.
    goto :fail
)

echo.
echo [3/5] Building UEFI bootloader...
echo.

call build -p "!PROJECT_DIR!\JollyOS.dsc" -a X64 -t VS2026 -b DEBUG

if errorlevel 1 (
    echo ERROR: UEFI bootloader build failed.
    goto :fail
)

if not exist "!BOOTLOADER_OUTPUT!" (
    echo ERROR: Bootloader.efi was not found.
    echo Expected: !BOOTLOADER_OUTPUT!
    goto :fail
)

echo.
echo [4/5] Building kernel...
echo.

call "!PROJECT_DIR!\Scripts\build-kernel.bat"

if errorlevel 1 (
    echo ERROR: Kernel build failed.
    goto :fail
)

if not exist "!KERNEL_OUTPUT!" (
    echo ERROR: kernel.exe was not created.
    echo Expected: !KERNEL_OUTPUT!
    goto :fail
)

echo.
echo [5/5] Preparing boot files...
echo.

if not exist "!PROJECT_DIR!\build\EFI\BOOT" (
    mkdir "!PROJECT_DIR!\build\EFI\BOOT"
)

if not exist "!PROJECT_DIR!\build\EFI\BOOT" (
    echo ERROR: Could not create the EFI boot directory.
    goto :fail
)

copy /Y "!BOOTLOADER_OUTPUT!" "!BOOTLOADER_DEST!"

if errorlevel 1 (
    echo ERROR: Could not copy Bootloader.efi.
    goto :fail
)

if not exist "!BOOTLOADER_DEST!" (
    echo ERROR: Bootloader copy could not be verified.
    goto :fail
)

echo.
echo ==========================================
echo          JollyOS BUILD SUCCEEDED
echo ==========================================
echo.
echo Bootloader:
echo !BOOTLOADER_DEST!
echo.
echo Kernel:
echo !KERNEL_OUTPUT!
echo.
echo NOTE: The VMware VHD/VMDK has NOT been updated.
echo.

pause
exit /b 0

:fail
echo.
echo ==========================================
echo             BUILD FAILED
echo ==========================================
echo.
echo Read the error messages above.
echo.
exit /b 1