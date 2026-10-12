@echo off
setlocal EnableExtensions

title JollyOS - Update VMware Image

echo ==========================================
echo      JollyOS VMware Image Update
echo ==========================================
echo.

set "PROJECT_DIR=F:\JollyOs"
set "BUILD_DIR=%PROJECT_DIR%\build"
set "VHD_PATH=%BUILD_DIR%\JollyOS.vhd"
set "VMDK_PATH=%BUILD_DIR%\JollyOS.vmdk"
set "TEMP_VMDK=%BUILD_DIR%\JollyOS.new.vmdk"
set "BACKUP_VMDK=%BUILD_DIR%\JollyOS.previous.vmdk"
set "DISKPART_SCRIPT=%TEMP%\JollyOS-diskpart-%RANDOM%.txt"
set "DRIVE=M:"
set "DISK_ATTACHED=0"

set "BOOTLOADER=%BUILD_DIR%\EFI\BOOT\BOOTX64.EFI"
set "KERNEL=%BUILD_DIR%\kernel.exe"
set "VMWARE_DISK_MANAGER=C:\Program Files\VMware\VMware Workstation\vmware-vdiskmanager.exe"

echo Checking prerequisites...

if not exist "%VHD_PATH%" (
    echo ERROR: Source VHD was not found:
    echo %VHD_PATH%
    goto :fail
)

if not exist "%BOOTLOADER%" (
    echo ERROR: Bootloader was not found:
    echo %BOOTLOADER%
    echo Run build-all.bat first.
    goto :fail
)

if not exist "%KERNEL%" (
    echo ERROR: Kernel was not found:
    echo %KERNEL%
    echo Run build-all.bat first.
    goto :fail
)

if not exist "%VMWARE_DISK_MANAGER%" (
    echo ERROR: VMware disk manager was not found:
    echo %VMWARE_DISK_MANAGER%
    goto :fail
)

if exist "%TEMP_VMDK%" (
    echo ERROR: A previous temporary VMDK already exists:
    echo %TEMP_VMDK%
    echo Check it before deleting or renaming it.
    goto :fail
)

echo.
echo Checking VMware is shut down...

tasklist /FI "IMAGENAME eq vmware-vmx.exe" 2>nul | find /I "vmware-vmx.exe" >nul

if not errorlevel 1 (
    echo ERROR: VMware is running a virtual machine.
    echo Shut down all VMs completely and try again.
    goto :fail
)

echo.
echo Checking drive letter %DRIVE% is available...

if exist "%DRIVE%\" (
    echo ERROR: Drive %DRIVE% is already in use.
    echo Free this drive letter or change DRIVE in this script.
    goto :fail
)

echo.
echo [1/4] Attaching source VHD...

(
    echo select vdisk file="%VHD_PATH%"
    echo attach vdisk
    echo select partition 1
    echo assign letter=M
    echo exit
) > "%DISKPART_SCRIPT%"

diskpart /s "%DISKPART_SCRIPT%"

if errorlevel 1 (
    echo ERROR: DiskPart reported a problem attaching the VHD.
    goto :fail
)

del "%DISKPART_SCRIPT%" >nul 2>&1
set "DISKPART_SCRIPT="

if not exist "M:\EFI\BOOT\." (
    echo ERROR: EFI boot folder was not found on J:.
    set "DISK_ATTACHED=1"
    goto :fail
)

if not exist "M:\kernel.exe" (
    echo ERROR: kernel.exe was not found on the mounted VHD.
    set "DISK_ATTACHED=1"
    goto :fail
)

set "DISK_ATTACHED=1"

echo.
echo [2/4] Copying updated boot files...

copy /Y "%BOOTLOADER%" "M:\EFI\BOOT\BOOTX64.EFI"

if errorlevel 1 (
    echo ERROR: Failed to copy the UEFI bootloader.
    goto :fail
)

copy /Y "%KERNEL%" "M:\kernel.exe"

if errorlevel 1 (
    echo ERROR: Failed to copy the kernel.
    goto :fail
)

echo.
echo [3/4] Detaching source VHD...

call :detach_vhd

if errorlevel 1 (
    echo ERROR: Could not confirm VHD detachment.
    goto :fail
)

echo.
echo [4/4] Creating replacement VMDK...

"%VMWARE_DISK_MANAGER%" -r "%VHD_PATH%" -t 0 "%TEMP_VMDK%"

if errorlevel 1 (
    echo ERROR: VMware disk conversion failed.
    goto :fail
)

if not exist "%TEMP_VMDK%" (
    echo ERROR: Conversion did not create the expected VMDK.
    goto :fail
)

echo.
echo Installing the new VMDK...

if exist "%BACKUP_VMDK%" (
    echo ERROR: Backup file already exists:
    echo %BACKUP_VMDK%
    echo Check it before proceeding.
    goto :fail
)

if exist "%VMDK_PATH%" (
    move "%VMDK_PATH%" "%BACKUP_VMDK%" >nul

    if errorlevel 1 (
        echo ERROR: Could not back up the existing VMDK.
        goto :fail
    )
)

move "%TEMP_VMDK%" "%VMDK_PATH%" >nul

if errorlevel 1 (
    echo ERROR: Could not install the replacement VMDK.

    if exist "%BACKUP_VMDK%" (
        move "%BACKUP_VMDK%" "%VMDK_PATH%" >nul
    )

    goto :fail
)

echo.
echo ==========================================
echo       VMWARE IMAGE UPDATE SUCCEEDED
echo ==========================================
echo.
echo Updated bootloader and kernel in:
echo %VHD_PATH%
echo.
echo New VMware disk:
echo %VMDK_PATH%
echo.
echo Previous disk backup, if one existed:
echo %BACKUP_VMDK%
echo.
echo Attach the updated VMDK to your JollyOS VM.
goto :success


:detach_vhd
if not "%DISKPART_SCRIPT%"=="" del "%DISKPART_SCRIPT%" >nul 2>&1

(
    echo select vdisk file="%VHD_PATH%"
    echo detach vdisk
    echo exit
) > "%TEMP%\JollyOS-detach.txt"

diskpart /s "%TEMP%\JollyOS-detach.txt"
del "%TEMP%\JollyOS-detach.txt" >nul 2>&1

if exist "M:\." (
    echo WARNING: Drive J: still appears to be accessible.
    exit /b 1
)

set "DISK_ATTACHED=0"
exit /b 0


:fail
echo.
echo ==========================================
echo              UPDATE FAILED
echo ==========================================

if "%DISK_ATTACHED%"=="1" (
    echo.
    echo Attempting to detach the source VHD...
    call :detach_vhd
)

if not "%DISKPART_SCRIPT%"=="" (
    del "%DISKPART_SCRIPT%" >nul 2>&1
)

echo.
echo Review the error above before running again.
goto :finish


:success
echo.
goto :finish


:finish
echo Press any key to close this window...
pause >nul
exit /b

