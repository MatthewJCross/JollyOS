#include <Uefi.h>

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>

#include <Guid/FileInfo.h>

#include <JollyOsBootInfo.h>

#define KERNEL_LOAD_ADDRESS 0x100000ULL
#define PE_SIGNATURE 0x00004550U
#define PE_MACHINE_AMD64 0x8664U
#define PE_MAGIC_PE32_PLUS 0x020BU

#define PE_DOS_HEADER_OFFSET 0x3CU
#define PE_FILE_HEADER_SIZE 20U
#define PE_SECTION_HEADER_SIZE 40U
#define PE_OPTIONAL_HEADER_MIN_SIZE 112U

typedef struct
{
    UINT64 Framebuffer;
    UINT32 Width;
    UINT32 Height;
    UINT32 PixelsPerScanLine;
    UINT32 PixelFormat;
} JOLLY_KERNEL_ARGUMENTS;

typedef VOID(*JOLLY_KERNEL_ENTRY)(const JOLLYOS_BOOT_INFO* BootInfo);

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/GraphicsOutput.h>

static EFI_STATUS ReadKernelFile(EFI_FILE_PROTOCOL* Root, VOID** FileBuffer, UINTN* FileSize)
{
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL* File;
    EFI_FILE_INFO* FileInfo;
    UINTN FileInfoSize;
    VOID* Buffer;
    UINTN BufferSize;

    File = NULL;
    FileInfo = NULL;
    Buffer = NULL;

    Status = Root->Open(Root, &File, L"\\kernel.exe", EFI_FILE_MODE_READ, 0);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Cannot open \\kernel.exe: %r\r\n", Status);
        return Status;
    }

    FileInfoSize = 0;

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, NULL);

    if (Status != EFI_BUFFER_TOO_SMALL)
    {
        File->Close(File);
        return EFI_ERROR(Status) ? Status : EFI_LOAD_ERROR;
    }

    Status = gBS->AllocatePool(EfiLoaderData, FileInfoSize, (VOID**)&FileInfo);

    if (EFI_ERROR(Status))
    {
        File->Close(File);
        return Status;
    }

    Status = File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);

    if (EFI_ERROR(Status))
    {
        gBS->FreePool(FileInfo);
        File->Close(File);
        return Status;
    }

    if (FileInfo->FileSize == 0 || FileInfo->FileSize > MAX_UINTN)
    {
        gBS->FreePool(FileInfo);
        File->Close(File);
        return EFI_LOAD_ERROR;
    }

    BufferSize = (UINTN)FileInfo->FileSize;

    gBS->FreePool(FileInfo);

    Status = gBS->AllocatePool(EfiLoaderData, BufferSize, &Buffer);

    if (EFI_ERROR(Status))
    {
        File->Close(File);
        return Status;
    }

    Status = File->Read(File, &BufferSize, Buffer);

    File->Close(File);

    if (EFI_ERROR(Status) || BufferSize == 0)
    {
        gBS->FreePool(Buffer);
        return EFI_ERROR(Status) ? Status : EFI_LOAD_ERROR;
    }

    *FileBuffer = Buffer;
    *FileSize = BufferSize;

    return EFI_SUCCESS;
}

static EFI_STATUS LoadKernelImage(const VOID* FileBuffer, UINTN FileSize, EFI_PHYSICAL_ADDRESS* ImageBase, UINTN* ImageSize, UINTN* EntryPointRva)
{
    const UINT8* FileBytes;
    const UINT8* NtHeaders;
    const UINT8* OptionalHeader;
    const UINT8* SectionHeader;
    UINT32 NtOffset;
    UINT16 NumberOfSections;
    UINT16 OptionalHeaderSize;
    UINT16 Machine;
    UINT16 OptionalMagic;
    UINT64 PreferredImageBase;
    UINT32 SizeOfImage;
    UINT32 SizeOfHeaders;
    UINT32 EntryRva;
    UINTN SectionTableOffset;
    UINTN SectionTableSize;
    UINTN Pages;
    UINTN Index;
    UINTN CopySize;
    UINT32 VirtualAddress;
    UINT32 VirtualSize;
    UINT32 RawSize;
    UINT32 RawOffset;
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS RequestedBase;
    UINT8* LoadedImage;

    if (FileBuffer == NULL || FileSize < 64)
    {
        return EFI_LOAD_ERROR;
    }

    FileBytes = (const UINT8*)FileBuffer;

    if (FileBytes[0] != 'M' || FileBytes[1] != 'Z')
    {
        Print(L"JollyOS: Kernel has no DOS/PE header.\r\n");
        return EFI_LOAD_ERROR;
    }

    NtOffset = *(const UINT32*)(FileBytes + PE_DOS_HEADER_OFFSET);

    if (NtOffset > FileSize || FileSize - NtOffset < 24)
    {
        return EFI_LOAD_ERROR;
    }

    NtHeaders = FileBytes + NtOffset;

    if (*(const UINT32*)NtHeaders != PE_SIGNATURE)
    {
        Print(L"JollyOS: Invalid PE signature.\r\n");
        return EFI_LOAD_ERROR;
    }

    Machine = *(const UINT16*)(NtHeaders + 4);

    if (Machine != PE_MACHINE_AMD64)
    {
        Print(L"JollyOS: Kernel is not an x64 image.\r\n");
        return EFI_UNSUPPORTED;
    }

    NumberOfSections = *(const UINT16*)(NtHeaders + 6);
    OptionalHeaderSize = *(const UINT16*)(NtHeaders + 20);

    if (NumberOfSections == 0 || OptionalHeaderSize < PE_OPTIONAL_HEADER_MIN_SIZE)
    {
        return EFI_LOAD_ERROR;
    }

    OptionalHeader = NtHeaders + 24;

    if ((UINTN)(OptionalHeader - FileBytes) > FileSize || OptionalHeaderSize > FileSize - (UINTN)(OptionalHeader - FileBytes))
    {
        return EFI_LOAD_ERROR;
    }

    OptionalMagic = *(const UINT16*)OptionalHeader;

    if (OptionalMagic != PE_MAGIC_PE32_PLUS)
    {
        return EFI_UNSUPPORTED;
    }

    EntryRva = *(const UINT32*)(OptionalHeader + 16);
    PreferredImageBase = *(const UINT64*)(OptionalHeader + 24);
    SizeOfImage = *(const UINT32*)(OptionalHeader + 56);
    SizeOfHeaders = *(const UINT32*)(OptionalHeader + 60);

    if (PreferredImageBase != KERNEL_LOAD_ADDRESS || SizeOfImage == 0 || SizeOfHeaders == 0 || SizeOfHeaders > FileSize || EntryRva >= SizeOfImage)
    {
        Print(L"JollyOS: Kernel image layout is not supported.\r\n");
        return EFI_LOAD_ERROR;
    }

    SectionTableOffset = (UINTN)(OptionalHeader - FileBytes) + OptionalHeaderSize;

    SectionTableSize = (UINTN)NumberOfSections * PE_SECTION_HEADER_SIZE;

    if (SectionTableOffset > FileSize || SectionTableSize > FileSize - SectionTableOffset)
    {
        return EFI_LOAD_ERROR;
    }

    for (Index = 0; Index < NumberOfSections; Index++)
    {
        SectionHeader = FileBytes + SectionTableOffset + Index * PE_SECTION_HEADER_SIZE;

        VirtualSize = *(const UINT32*)(SectionHeader + 8);
        VirtualAddress = *(const UINT32*)(SectionHeader + 12);
        RawSize = *(const UINT32*)(SectionHeader + 16);
        RawOffset = *(const UINT32*)(SectionHeader + 20);

        CopySize = RawSize;

        if (CopySize > FileSize || RawOffset > FileSize || CopySize > FileSize - RawOffset || VirtualAddress > SizeOfImage || CopySize > SizeOfImage - VirtualAddress || VirtualSize > SizeOfImage - VirtualAddress)
        {
            return EFI_LOAD_ERROR;
        }
    }

    Pages = EFI_SIZE_TO_PAGES(SizeOfImage);
    RequestedBase = KERNEL_LOAD_ADDRESS;

    Status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, Pages, &RequestedBase);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Cannot allocate kernel memory at 0x%lx: %r\r\n", (UINT64)KERNEL_LOAD_ADDRESS, Status);

        return Status;
    }

    LoadedImage = (UINT8*)(UINTN)RequestedBase;

    SetMem(LoadedImage, Pages * EFI_PAGE_SIZE, 0);
    CopyMem(LoadedImage, FileBytes, SizeOfHeaders);

    for (Index = 0; Index < NumberOfSections; Index++)
    {
        SectionHeader = FileBytes + SectionTableOffset + Index * PE_SECTION_HEADER_SIZE;

        VirtualAddress = *(const UINT32*)(SectionHeader + 12);
        RawSize = *(const UINT32*)(SectionHeader + 16);
        RawOffset = *(const UINT32*)(SectionHeader + 20);

        if (RawSize != 0)
        {
            CopyMem(LoadedImage + VirtualAddress, FileBytes + RawOffset, RawSize);
        }
    }

    *ImageBase = RequestedBase;
    *ImageSize = SizeOfImage;
    *EntryPointRva = EntryRva;

    return EFI_SUCCESS;
}

static EFI_STATUS GetGraphicsInfo(EFI_GRAPHICS_OUTPUT_PROTOCOL** Graphics, UINT32* PixelFormat)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop;

    Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&Gop);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Graphics Output Protocol unavailable.\r\n");
        return Status;
    }

    if (Gop->Mode == NULL || Gop->Mode->Info == NULL)
    {
        return EFI_UNSUPPORTED;
    }

    if (Gop->Mode->Info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor && Gop->Mode->Info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor)
    {
        Print(L"JollyOS: Unsupported framebuffer pixel format.\r\n");
        return EFI_UNSUPPORTED;
    }

    *Graphics = Gop;
    *PixelFormat = (UINT32)Gop->Mode->Info->PixelFormat;

    return EFI_SUCCESS;
}



static EFI_STATUS ExitBootServicesAndGetMap(EFI_HANDLE ImageHandle, EFI_MEMORY_DESCRIPTOR** MemoryMap, UINTN* MemoryMapSize, UINTN* DescriptorSize, UINT32* DescriptorVersion, UINTN* MapKey)
{
    EFI_STATUS Status;
    UINTN RequiredSize;
    UINTN Capacity;
    EFI_MEMORY_DESCRIPTOR* Buffer;
    UINTN ExtraCapacity;
    UINTN RetryCount;

    Buffer = NULL;
    Capacity = 0;
    RetryCount = 0;

    for (;;)
    {
        RequiredSize = Capacity;

        Status = gBS->GetMemoryMap(&RequiredSize, Buffer, MapKey, DescriptorSize, DescriptorVersion);

        if (Status == EFI_BUFFER_TOO_SMALL)
        {
            if (Buffer != NULL)
            {
                gBS->FreePool(Buffer);
                Buffer = NULL;
            }

            ExtraCapacity = 2 * (*DescriptorSize == 0 ? sizeof(EFI_MEMORY_DESCRIPTOR) : *DescriptorSize);

            if (RequiredSize > MAX_UINTN - ExtraCapacity)
            {
                return EFI_OUT_OF_RESOURCES;
            }

            Capacity = RequiredSize + ExtraCapacity;

            Status = gBS->AllocatePool(EfiLoaderData, Capacity, (VOID**)&Buffer);

            if (EFI_ERROR(Status))
            {
                return Status;
            }

            continue;
        }

        if (EFI_ERROR(Status))
        {
            if (Buffer != NULL)
            {
                gBS->FreePool(Buffer);
            }

            return Status;
        }

        Status = gBS->ExitBootServices(ImageHandle, *MapKey);

        if (!EFI_ERROR(Status))
        {
            *MemoryMap = Buffer;
            *MemoryMapSize = RequiredSize;

            return EFI_SUCCESS;
        }

        if (Status != EFI_INVALID_PARAMETER)
        {
            return Status;
        }

        RetryCount++;

        if (RetryCount >= 5)
        {
            return EFI_ABORTED;
        }
    }
}

EFI_STATUS SetPreferredGraphicsMode(EFI_GRAPHICS_OUTPUT_PROTOCOL *Graphics, UINT32 Width, UINT32 Height)
{
    if (Graphics == NULL || Graphics->Mode == NULL)
    {
        return EFI_INVALID_PARAMETER;
    }

    for (UINT32 ModeNumber = 0; ModeNumber < Graphics->Mode->MaxMode; ModeNumber++)
    {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *ModeInfo = NULL;
        UINTN ModeInfoSize = 0;

        EFI_STATUS Status = Graphics->QueryMode(Graphics, ModeNumber, &ModeInfoSize, &ModeInfo);

        if (EFI_ERROR(Status))
        {
            continue;
        }

        BOOLEAN Match = ModeInfo->HorizontalResolution == Width &&
                        ModeInfo->VerticalResolution == Height;

        FreePool(ModeInfo);

        if (Match)
        {
            return Graphics->SetMode(Graphics, ModeNumber);
        }
    }

    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    UINT32 DescriptorVersion;
    JOLLYOS_BOOT_INFO* BootInfo;
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImageProtocol;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
    EFI_FILE_PROTOCOL* Root;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Graphics;
    VOID* KernelFile;
    UINTN KernelFileSize;
    EFI_PHYSICAL_ADDRESS KernelBase;
    UINTN KernelSize;
    UINTN EntryPointRva;
    UINT32 PixelFormat;
    UINT32 Width;
    UINT32 Height;
    UINT32 PixelsPerScanLine;
    EFI_PHYSICAL_ADDRESS FramebufferBase;
    EFI_MEMORY_DESCRIPTOR* MemoryMap;
    UINTN MemoryMapSize;
    UINTN DescriptorSize;
    UINTN MapKey;
    JOLLY_KERNEL_ENTRY KernelEntry;

    (VOID)SystemTable;

    Print(L"\r\n");
    Print(L"==============================\r\n");
    Print(L"       JOLLYOS BOOTLOADER\r\n");
    Print(L"==============================\r\n\r\n");

    Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImageProtocol);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Cannot identify boot volume.\r\n");
        return Status;
    }

    Status = gBS->HandleProtocol(LoadedImageProtocol->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&FileSystem);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Boot volume has no filesystem protocol.\r\n");
        return Status;
    }

    Status = FileSystem->OpenVolume(FileSystem, &Root);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Cannot open boot volume.\r\n");
        return Status;
    }

    KernelFile = NULL;
    KernelFileSize = 0;

    Status = ReadKernelFile(Root, &KernelFile, &KernelFileSize);

    Root->Close(Root);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Kernel file could not be read.\r\n");
        return Status;
    }

    Print(L"JollyOS: Kernel file loaded into temporary memory.\r\n");

    Status = LoadKernelImage(KernelFile, KernelFileSize, &KernelBase, &KernelSize, &EntryPointRva);

    gBS->FreePool(KernelFile);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Kernel image loading failed: %r\r\n", Status);
        return Status;
    }

    Print(L"JollyOS: Kernel image mapped at 0x%lx.\r\n", KernelBase);

    Status = GetGraphicsInfo(&Graphics, &PixelFormat);

    if (EFI_ERROR(Status))
    {
        return Status;
    }

	Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,  NULL,	(VOID **)&Graphics);
	if (EFI_ERROR(Status) || Graphics == NULL || Graphics->Mode == NULL || Graphics->Mode->Info == NULL)
	{
	    Print(L"ERROR: Unable to locate UEFI Graphics Output Protocol.\r\n");
	    return EFI_UNSUPPORTED;
	}

	EFI_STATUS GraphicsStatus = SetPreferredGraphicsMode(Graphics, 1280, 1024);

	if (EFI_ERROR(GraphicsStatus))
	{
	    Print(L"1280 x 1024 unavailable; retaining current graphics mode.\r\n");
	}
	else
	{
	    Print(L"Graphics mode set to 1280 x 1024.\r\n");
	}

	Width = Graphics->Mode->Info->HorizontalResolution;
	Height = Graphics->Mode->Info->VerticalResolution;
	PixelsPerScanLine = Graphics->Mode->Info->PixelsPerScanLine;
	FramebufferBase = Graphics->Mode->FrameBufferBase;

	Print(L"Framebuffer: %u x %u\r\n", Width, Height);
    Print(L"JollyOS: Exiting UEFI boot services...\r\n");

    /*
     * Allocate and populate the boot-information structure before
     * ExitBootServices. It remains allocated for the kernel to use.
     */
    BootInfo = NULL;

    Status = gBS->AllocatePool(EfiLoaderData, sizeof(JOLLYOS_BOOT_INFO), (VOID**)&BootInfo);

    if (EFI_ERROR(Status))
    {
        Print(L"JollyOS: Cannot allocate boot information: %r\r\n", Status);
        return Status;
    }

    ZeroMem(BootInfo, sizeof(JOLLYOS_BOOT_INFO));

    BootInfo->Version = JOLLYOS_BOOT_INFO_VERSION;
    BootInfo->Size = (UINT32)sizeof(JOLLYOS_BOOT_INFO);

    BootInfo->Framebuffer = (UINT64)FramebufferBase;
    BootInfo->Width = Width;
    BootInfo->Height = Height;
    BootInfo->PixelsPerScanLine = PixelsPerScanLine;
    BootInfo->PixelFormat = PixelFormat;

    BootInfo->KernelBase = (UINT64)KernelBase;
    BootInfo->KernelSize = (UINT64)KernelSize;

    MemoryMap = NULL;
    MemoryMapSize = 0;
    DescriptorSize = 0;
    DescriptorVersion = 0;
    MapKey = 0;

    KernelEntry = (JOLLY_KERNEL_ENTRY)(UINTN)(KernelBase + EntryPointRva);

    Status = ExitBootServicesAndGetMap(ImageHandle, &MemoryMap, &MemoryMapSize, &DescriptorSize, &DescriptorVersion, &MapKey);

    if (EFI_ERROR(Status))
    {
        /*
         * Do not use the framebuffer or make further boot-service
         * calls here. The firmware may have partially shut down
         * boot services after an unsuccessful exit attempt.
         */
        return Status;
    }

    /*
     * These are ordinary memory writes. No UEFI boot services are
     * called after ExitBootServices succeeds.
     */
    BootInfo->MemoryMap = (UINT64)(UINTN)MemoryMap;
    BootInfo->MemoryMapSize = (UINT64)MemoryMapSize;
    BootInfo->MemoryMapDescriptorSize = (UINT64)DescriptorSize;
    BootInfo->MemoryMapDescriptorVersion = DescriptorVersion;

    /*
     * Transfer control to the kernel. The kernel must not return.
     */
    KernelEntry(BootInfo);

    CpuDeadLoop();

    return EFI_SUCCESS;
}