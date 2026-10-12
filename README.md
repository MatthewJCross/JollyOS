# JollyOS

JollyOS is an experimental x86-64 operating system being developed using C, assembly language, and UEFI. The aim is to build an operating system from the ground up, starting with a custom bootloader and kernel and gradually adding memory management, hardware support, and an interactive command prompt.

The project has reached its first major milestone: **JollyOS boots in VMware Workstation and displays its own welcome screen using a custom UEFI bootloader and C kernel.**

## 1. Current status

The current boot screen displays:

* `WELCOME TO JOLLYOS!`
* `BOOTED FROM OUR OWN UEFI LOADER.`

The successful boot demonstrates that:

* VMware UEFI firmware can launch the JollyOS bootloader.
* The bootloader can locate and load the kernel.
* The bootloader can obtain the UEFI graphics framebuffer.
* The bootloader can exit UEFI boot services.
* Control can transfer to the kernel's assembly entry point.
* The C kernel can draw directly to the framebuffer.

JollyOS is currently a minimal proof of concept. It does not yet have a memory manager, keyboard input, a shell, a filesystem, multitasking, or CPU exception reporting.

## 2. Development environment

The project is built natively on Windows. WSL is not required for the current workflow.

| Component           | Configuration                                                        |
| ------------------- | -------------------------------------------------------------------- |
| Project directory   | `F:\JollyOs`                                                         |
| EDK II source       | `C:\edk2`                                                            |
| IDE/toolchain       | Visual Studio 2026 Professional                                      |
| MSVC toolset        | `14.52.36725`                                                        |
| Target architecture | x86-64                                                               |
| Python              | Python 3.13, selected with `py -3.13`                                |
| NASM                | `C:\Program Files\NASM\nasm.exe`                                     |
| VMware Workstation  | Used to run the OS                                                   |
| VMware disk utility | `C:\Program Files\VMware\VMware Workstation\vmware-vdiskmanager.exe` |

The EDK II build uses the `VS2026` toolchain tag.

## 3. Project structure

The main project structure is:

```text
F:\JollyOs\
  README.md
  JollyOS.dsc
  JollyOS.dec
  Include\
  Bootloader\
    Bootloader.inf
    Bootloader.c
  Kernel\
    kernel.c
    entry.asm
    linker.ld
  Scripts\
    build-kernel.bat
    build-all.bat
    update-vmware-image.bat
  build\
    entry.obj
    kernel.obj
    kernel.exe
    EFI\
      BOOT\
        BOOTX64.EFI
    JollyOS.vhd
    JollyOS.vmdk
```

Some files in `build` are generated outputs. `linker.ld` is a legacy file from the earlier build approach and is not used by the current MSVC kernel build.

The important source files are:

* `Bootloader\Bootloader.c` — UEFI bootloader implementation.
* `Kernel\entry.asm` — assembly entry point for the kernel.
* `Kernel\kernel.c` — freestanding C kernel and framebuffer drawing.
* `JollyOS.dsc` — EDK II platform build configuration.
* `JollyOS.dec` — JollyOS package definition.
* `Scripts\build-kernel.bat` — builds the kernel.
* `Scripts\build-all.bat` — initialises the build environments and builds both components.
* `Scripts\update-vmware-image.bat` — updates the VMware boot image.

## 4. How the boot process works

The current boot process is:

1. VMware Workstation starts the virtual machine using UEFI firmware.
2. UEFI launches `\EFI\BOOT\BOOTX64.EFI` from the EFI System Partition.
3. The bootloader opens `\kernel.exe` from the same partition.
4. The bootloader validates the kernel's x64 PE image.
5. The kernel image is loaded at the fixed address `0x100000`.
6. The bootloader obtains the framebuffer address, screen dimensions, pixels per scan line, and pixel format from the UEFI Graphics Output Protocol (GOP).
7. The bootloader obtains the UEFI memory map.
8. The bootloader calls `ExitBootServices()`.
9. The bootloader transfers control to the kernel entry point.
10. The assembly entry point calls `kernel_main` using the Microsoft x64 calling convention.
11. The C kernel initialises its framebuffer information, draws the welcome screen, and halts in a loop.

The kernel is currently built as a fixed-address PE image. The bootloader manually maps its headers and sections into memory. This is not yet a general-purpose executable loader: the kernel depends on the expected load address and does not currently use normal PE relocation or import processing.

## 5. Building the UEFI bootloader

EDK II is installed at:

```text
C:\edk2
```

The JollyOS platform description is:

```text
F:\JollyOs\JollyOS.dsc
```

The EDK II build command is:

```bat
build -p F:\JollyOs\JollyOS.dsc -a X64 -t VS2026 -b DEBUG
```

The main build settings are:

* Architecture: `X64`
* Toolchain: `VS2026`
* Build target: `DEBUG`
* Module type: `UEFI_APPLICATION`

The bootloader build produces `Bootloader.efi`. The output used by the current build script is:

```text
C:\edk2\Build\JollyOS\DEBUG_VS2026\X64\Bootloader\Bootloader\OUTPUT\Bootloader.efi
```

The bootloader is copied to:

```text
F:\JollyOs\build\EFI\BOOT\BOOTX64.EFI
```

The filename `BOOTX64.EFI` is the standard removable-media boot filename for x86-64 UEFI systems.

## 6. Building the kernel

The kernel is compiled using the Visual Studio x64 C compiler and MASM assembler.

The script `Scripts\build-kernel.bat` performs three steps:

1. Assemble `Kernel\entry.asm` with `ml64.exe`.
2. Compile `Kernel\kernel.c` with `cl.exe`.
3. Link the object files with `link.exe` to produce `kernel.exe`.

The kernel build uses freestanding-oriented options, including:

* `/GS-` — disables the compiler's security-cookie instrumentation.
* `/Zl` — omits default library name directives.
* `/NODEFAULTLIB` — prevents automatic linking of default libraries.
* `/ENTRY:_start` — sets the custom assembly entry point.
* `/BASE:0x100000` — sets the preferred image base.
* `/FIXED` and `/DYNAMICBASE:NO` — use the current fixed-address design.

The output is:

```text
F:\JollyOs\build\kernel.exe
```

The kernel is not a conventional Windows application, even though its intermediate image uses the PE executable format.

### Building the kernel manually

If building the kernel independently, initialise the Visual Studio x64 environment first:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"
F:\JollyOs\Scripts\build-kernel.bat
```

The kernel build script checks that `cl.exe`, `ml64.exe`, and `link.exe` are available.

## 7. Building everything with one script

The script:

```text
F:\JollyOs\Scripts\build-all.bat
```

is intended to provide a single build entry point.

It performs the following tasks:

1. Checks the project, Visual Studio, EDK II, and NASM paths.
2. Initialises the Visual Studio x64 environment.
3. Initialises EDK II.
4. Builds the UEFI bootloader.
5. Builds the kernel.
6. Copies the generated bootloader into the local build directory.

Run it from a normal Command Prompt:

```bat
F:\JollyOs\Scripts\build-all.bat
```

It can also be launched from File Explorer. The script should pause at the end so that its output remains visible.

The build script updates the local build files only. It does not automatically update the VHD or VMDK.

## 8. Creating the VMware boot disk

The working test image uses a 128 MB fixed-size VHD as the source disk and a VMDK for VMware Workstation.

The VHD was created with Windows DiskPart, initialised as GPT, and configured with a 100 MB FAT32 EFI System Partition.

The EFI partition contains:

```text
\EFI\BOOT\BOOTX64.EFI
\kernel.exe
```

The bootloader loads `kernel.exe` from the root of the EFI partition.

The source VHD is:

```text
F:\JollyOs\build\JollyOS.vhd
```

The VMware disk is:

```text
F:\JollyOs\build\JollyOS.vmdk
```

The VHD is converted to a VMware disk using:

```bat
"C:\Program Files\VMware\VMware Workstation\vmware-vdiskmanager.exe" -r "F:\JollyOs\build\JollyOS.vhd" -t 0 "F:\JollyOs\build\JollyOS.vmdk"
```

The `-t 0` option creates a growable virtual disk.

**Important:** rebuilding `kernel.exe` or `BOOTX64.EFI` does not automatically update the files inside an existing virtual disk.

## 9. Updating the VMware image

The script:

```text
F:\JollyOs\Scripts\update-vmware-image.bat
```

automates updating the boot image.

Its intended process is:

1. Check that the local bootloader and kernel exist.
2. Check that VMware is not running a virtual machine.
3. Attach `JollyOS.vhd`.
4. Copy the latest bootloader and kernel into the EFI partition.
5. Detach the VHD.
6. Convert the updated VHD into a replacement VMDK.
7. Keep the previous VMDK as a backup when the replacement succeeds.

The mounted VHD uses drive letter **`M:`** on this machine. The script must consistently use `M:` for the assigned drive letter and all paths accessing the mounted partition.

### Recommended update workflow

1. Shut down JollyOS completely in VMware. Do not leave the VM suspended.
2. Run `build-all.bat`.
3. Run `update-vmware-image.bat`.
4. Wait for the update script to report success.
5. Start the JollyOS virtual machine using the updated VMDK.

Run the update script with sufficient permissions for DiskPart to attach and detach the VHD.

If the update fails, read the error carefully before trying again. Do not delete existing VHD, VMDK, or backup files blindly.

The script was written for the current machine and paths; its operation should be verified after changes to the disk layout or Windows configuration.

## 10. Kernel framebuffer implementation

The kernel currently uses a simple framebuffer drawing implementation in `Kernel\kernel.c`.

It maintains these values:

* Framebuffer address.
* Screen width and height.
* Pixels per scan line.
* UEFI framebuffer pixel format.

The `PutPixel` function writes individual pixels. `FillRectangle` draws filled rectangles, while `DrawText` uses a small 5-by-7 bitmap font.

The kernel draws:

* A dark background.
* A blue header bar.
* The JollyOS welcome message.
* A second line confirming that the system booted through its own UEFI loader.

The kernel then enters a halt loop.

The framebuffer is accessed directly through the address supplied by the bootloader. The kernel does not yet have a graphics driver abstraction or memory protection.

## 11. Troubleshooting

### VMware says the CPU has been disabled

This message can indicate a fatal guest CPU exception or triple fault.

A bootloader message immediately before the handover does not prove that the failure occurred inside `ExitBootServices()`. The failure could happen during the handover or in the first kernel instructions.

The welcome screen now confirms that the primary boot path can succeed. Preserve the working source and disk image before making changes to the entry point, image loader, or memory handling.

### The VM displays an old version

The build directory and virtual disk contain separate copies of the boot files.

Run both scripts in order:

```text
build-all.bat
update-vmware-image.bat
```

Then confirm that VMware is using:

```text
F:\JollyOs\build\JollyOS.vmdk
```

### The compiler cannot be found

The kernel script requires the Visual Studio x64 environment. Use `build-all.bat` or initialise the environment manually with:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"
```

### EDK II fails to build

Check that:

* `C:\edk2` exists.
* `F:\JollyOs\JollyOS.dsc` exists.
* The toolchain tag is `VS2026`.
* Python 3.13 is available through `py -3.13`.
* NASM is installed at `C:\Program Files\NASM\nasm.exe`.

Optional-tool warnings are not necessarily fatal. Check the actual build exit code and the first error message.

### Disk update fails

Check that:

* VMware has shut down all VMs.
* The VHD is not already mounted.
* Drive letter `M:` is available.
* The EFI partition contains `EFI\BOOT` and `kernel.exe`.
* The VMware disk utility exists at the configured path.

If a disk conversion fails, preserve the source VHD and any existing VMDK backup until the problem is understood.

## 12. Recommended next development milestones

The next stages should be implemented incrementally, testing in VMware after each major change.

1. **Serial debug output** — make early boot messages available independently of the framebuffer.
2. **CPU exception handlers** — report exceptions and useful register state rather than relying on VMware's CPU-disabled message.
3. **Memory map handling** — retain the UEFI memory map after exiting boot services.
4. **Physical memory manager** — allocate and track usable physical pages.
5. **Interrupt handling** — configure the processor's interrupt descriptor table and required hardware support.
6. **Keyboard input** — read input and display it on screen.
7. **Basic command prompt** — create an interactive environment for testing OS functionality.
8. **Filesystem support** — read and write files.
9. **Multitasking and system calls** — develop process and task support as the kernel architecture matures.
10. **Possible C# runtime** — consider a managed runtime only after the lower-level memory, execution, and runtime foundations are in place.

## 13. Backups and project safety

Before major changes, keep copies of:

* `Bootloader\Bootloader.c`
* `Kernel\entry.asm`
* `Kernel\kernel.c`
* `JollyOS.dsc`
* `JollyOS.dec`
* `build\JollyOS.vhd`
* `build\JollyOS.vmdk`

The working VMware image is a useful recovery point. Keep a known-good copy until a replacement has successfully booted.

## Summary

JollyOS has progressed from a Windows-native EDK II and MSVC build setup to a bootable x86-64 UEFI operating-system prototype.

The current system launches its own UEFI bootloader, loads a freestanding C kernel, exits boot services, and renders a welcome screen directly to the framebuffer in VMware Workstation.

The next priority is to add serial debugging and exception handling, then build the memory-management and input foundations needed for a more capable operating system.
