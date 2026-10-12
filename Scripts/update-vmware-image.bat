@echo off
setlocal EnableExtensions EnableDelayedExpansion

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
set "BOOTLOADER=%BUILD_DIR%\EFI\BOOT\BOOTX64.EFI"
set "KERNEL=%BUILD_DIR%\kernel.exe"
set "VMWARE_DISK_MANAGER=C:\Program Files\VMware\VMware Workstation\vmware-vdiskmanager.exe"

set "DRIVE=P:"
set "DISKPART_SCRIPT=%TEMP%\JollyOS-diskpart-%RANDOM%.txt"
set "DISKPART_OUTPUT=%TEMP%\JollyOS-diskpart-output-%RANDOM%.txt"

set "DISK_ATTACHED=0"
set "PARTITION_MODIFIED=0"

set "EFI_TYPE=C12A7328-F81F-11D2-BA4B-00A0C93EC93B"
set "BASIC_DATA_TYPE=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7"
set "EFI_ATTRIBUTES=0x8000000000000000"

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

if exist "%BACKUP_VMDK%" (
    echo ERROR: Backup file already exists:
    echo %BACKUP_VMDK%
    echo Check it before proceeding.
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
echo Checking temporary drive letter %DRIVE%...

mountvol %DRIVE% /L >nul 2>&1

if not errorlevel 1 (
    echo ERROR: Drive letter %DRIVE% is already mounted.
    echo Choose another unused drive letter before proceeding.
    goto :fail
)

echo.
echo [1/4] Attaching the source VHD...

(
    echo select vdisk file="%VHD_PATH%"
    echo attach vdisk
    echo exit
) > "%DISKPART_SCRIPT%"

diskpart /s "%DISKPART_SCRIPT%" > "%DISKPART_OUTPUT%" 2>&1
type "%DISKPART_OUTPUT%"

findstr /I /C:"DiskPart successfully attached the virtual disk file." "%DISKPART_OUTPUT%" >nul

if errorlevel 1 (
    echo ERROR: DiskPart did not confirm that the VHD was attached.
    goto :fail
)

set "DISK_ATTACHED=1"

echo.
echo Temporarily changing the VHD EFI partition type...

rem This modifies partition 1 on the selected VHD only.
rem The original EFI type and attributes are restored before detaching.

set "PARTITION_MODIFIED=1"

(
    echo select vdisk file="%VHD_PATH%"
    echo select partition 1
    echo set id=%BASIC_DATA_TYPE% override
    echo gpt attributes=0x0000000000000000
    echo assign letter=%DRIVE:~0,1%
    echo exit
) > "%DISKPART_SCRIPT%"

diskpart /s "%DISKPART_SCRIPT%" > "%DISKPART_OUTPUT%" 2>&1
type "%DISKPART_OUTPUT%"

findstr /I /C:"DiskPart successfully assigned the drive letter or mount point." "%DISKPART_OUTPUT%" >nul

if errorlevel 1 (
    echo ERROR: Could not assign the temporary drive letter.
    goto :fail
)

del "%DISKPART_SCRIPT%" >nul 2>&1
del "%DISKPART_OUTPUT%" >nul 2>&1

echo.
echo Checking the mounted partition...

if not exist "%DRIVE%\EFI\BOOT\BOOTX64.EFI" (
    echo ERROR: BOOTX64.EFI was not found:
    echo %DRIVE%\EFI\BOOT\BOOTX64.EFI
    goto :fail
)

if not exist "%DRIVE%\kernel.exe" (
    echo ERROR: kernel.exe was not found:
    echo %DRIVE%\kernel.exe
    goto :fail
)

echo.
echo [2/4] Copying updated boot files...

copy /Y "%BOOTLOADER%" "%DRIVE%\EFI\BOOT\BOOTX64.EFI"

if errorlevel 1 (
    echo ERROR: Failed to copy the UEFI bootloader.
    goto :fail
)

copy /Y "%KERNEL%" "%DRIVE%\kernel.exe"

if errorlevel 1 (
    echo ERROR: Failed to copy the kernel.
    goto :fail
)

echo.
echo [3/4] Restoring the EFI partition and detaching the VHD...

call :restore_and_detach

if errorlevel 1 (
    echo ERROR: Partition restoration or VHD detachment failed.
    echo Do not convert the VHD until its partition is restored.
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

if exist "%BACKUP_VMDK%" (
    echo Previous disk backup:
    echo %BACKUP_VMDK%
)

echo.
echo The original EFI partition type and attributes
echo have been restored, and the source VHD is detached.
echo Attach the updated VMDK to your JollyOS VM.
goto :success

:restore_and_detach

if "!DISK_ATTACHED!"=="1" (
    echo Restoring the original EFI partition type...

    (
        echo select vdisk file="%VHD_PATH%"
        echo select partition 1
        echo remove letter=%DRIVE:~0,1%
        echo set id=%EFI_TYPE% override
        echo gpt attributes=%EFI_ATTRIBUTES%
        echo detail partition
        echo detach vdisk
        echo exit
    ) > "%DISKPART_SCRIPT%"

    diskpart /s "%DISKPART_SCRIPT%" > "%DISKPART_OUTPUT%" 2>&1
    type "%DISKPART_OUTPUT%"

    findstr /I /C:"DiskPart successfully detached the virtual disk file." "%DISKPART_OUTPUT%" >nul

    if errorlevel 1 (
        echo ERROR: DiskPart did not confirm VHD detachment.
        exit /b 1
    )

    findstr /I /C:"%EFI_TYPE%" "%DISKPART_OUTPUT%" >nul

    if errorlevel 1 (
        echo ERROR: The original EFI partition type was not confirmed.
        exit /b 1
    )

    findstr /I /C:"0X8000000000000000" "%DISKPART_OUTPUT%" >nul

    if errorlevel 1 (
        echo ERROR: The original EFI partition attributes were not confirmed.
        exit /b 1
    )

    set "DISK_ATTACHED=0"
    set "PARTITION_MODIFIED=0"
)

del "%DISKPART_SCRIPT%" >nul 2>&1
del "%DISKPART_OUTPUT%" >nul 2>&1

exit /b 0

:success

del "%DISKPART_SCRIPT%" >nul 2>&1
del "%DISKPART_OUTPUT%" >nul 2>&1

echo.
pause
exit /b 0

:fail

echo.
echo ==========================================
echo             UPDATE FAILED
echo ==========================================
echo.

if "!DISK_ATTACHED!"=="1" (
    echo Attempting to restore the EFI partition and detach the VHD...
    call :restore_and_detach

    if errorlevel 1 (
        echo.
        echo WARNING: Automatic restoration or detachment failed.
        echo Do not run the update again until the VHD is inspected.
        echo Preserve the source VHD and all backup files.
    )
)

if defined DISKPART_SCRIPT del "%DISKPART_SCRIPT%" >nul 2>&1
if defined DISKPART_OUTPUT del "%DISKPART_OUTPUT%" >nul 2>&1

echo.
echo Review the errors above before retrying.
echo.
pause
exit /b 1
