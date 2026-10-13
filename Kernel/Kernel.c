#include <stdint.h>
#include <stddef.h>
#include <intrin.h>

#include "JollyOsBootInfo.h"
#include "PhysicalMemory.h"
#include "serial.h"
#include "VirtualMemory.h"

static volatile uint8_t *g_framebuffer;
static uint32_t g_width;
static uint32_t g_height;
static uint32_t g_pixelsPerScanLine;
static uint32_t g_pixelFormat;

#define JOLLYOS_PAGE_SIZE 4096ULL
#define JOLLYOS_PAGE_SHIFT 12U

static uint8_t* g_pageStateBitmap;
static uint8_t* g_managedPageBitmap;
static uint8_t* g_allocatorOwnedBitmap;
static uint64_t g_maximumPhysicalPage;
static uint64_t g_managedPageCount;
static uint64_t g_freePageCount;
static uint64_t g_allocatorOwnedPageCount;
static int g_physicalMemoryReady;

static void PutPixel(uint32_t x, uint32_t y, uint8_t red, uint8_t green, uint8_t blue)
{
	if (g_framebuffer == 0 || x >= g_width || y >= g_height)
	{
		return;
	}

	size_t offset = ((size_t)y * g_pixelsPerScanLine + x) * 4;

	volatile uint8_t *pixel = g_framebuffer + offset;

	if (g_pixelFormat == 1)
	{
	    pixel[0] = blue;
	    pixel[1] = green;
	    pixel[2] = red;
	}
	else
	{
	    pixel[0] = red;
	    pixel[1] = green;
	    pixel[2] = blue;
	}

	pixel[3] = 0;
}

static void FillRectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t red, uint8_t green, uint8_t blue)
{
	uint32_t endX = x + width;
	uint32_t endY = y + height;

	if (endX < x || endX > g_width)
	{
	    endX = g_width;
	}

	if (endY < y || endY > g_height)
	{
	    endY = g_height;
	}

	for (uint32_t py = y; py < endY; py++)
	{
	    for (uint32_t px = x; px < endX; px++)
	    {
	        PutPixel(px, py, red, green, blue);
	    }
	}
}

/*

* Each character uses a 5x7 bitmap.
* Characters are stored as five-bit rows.
  */
  static const uint8_t Font[][7] =
  {
	  {14, 17, 17, 31, 17, 17, 17}, /* A */
	  {30, 17, 17, 30, 17, 17, 30}, /* B */
	  {14, 17, 16, 16, 16, 17, 14}, /* C */
	  {30, 17, 17, 17, 17, 17, 30}, /* D */
	  {31, 16, 16, 30, 16, 16, 31}, /* E */
	  {31, 16, 16, 30, 16, 16, 16}, /* F */
	  {14, 17, 16, 23, 17, 17, 15}, /* G */
	  {17, 17, 17, 31, 17, 17, 17}, /* H */
	  {31, 4, 4, 4, 4, 4, 31},      /* I */
	  {7, 2, 2, 2, 18, 18, 12},     /* J */
	  {17, 18, 20, 24, 20, 18, 17}, /* K */
	  {16, 16, 16, 16, 16, 16, 31}, /* L */
	  {17, 27, 21, 21, 17, 17, 17}, /* M */
	  {17, 25, 21, 19, 17, 17, 17}, /* N */
	  {14, 17, 17, 17, 17, 17, 14}, /* O */
	  {30, 17, 17, 30, 16, 16, 16}, /* P */
	  {14, 17, 17, 17, 21, 18, 13}, /* Q */
	  {30, 17, 17, 30, 20, 18, 17}, /* R */
	  {15, 16, 16, 14, 1, 1, 30},   /* S */
	  {31, 4, 4, 4, 4, 4, 4},       /* T */
	  {17, 17, 17, 17, 17, 17, 14}, /* U */
	  {17, 17, 17, 17, 17, 10, 4},  /* V */
	  {17, 17, 17, 21, 21, 21, 10}, /* W */
	  {17, 17, 10, 4, 10, 17, 17},  /* X */
	  {17, 17, 10, 4, 4, 4, 4},     /* Y */
	  {31, 1, 2, 4, 8, 16, 31},      /* Z */
	  {0, 0, 0, 0, 0, 0, 0},         /* Space */
	  {14, 17, 1, 2, 4, 0, 4},       /* ! */
	  {0, 0, 0, 0, 0, 0, 0}          /* . */
  };

static int GetCharacterIndex(char character)
{
	if (character >= 'A' && character <= 'Z')
	{
		return character - 'A';
	}

	if (character == ' ')
	{
	    return 26;
	}

	if (character == '!')
	{
	    return 27;
	}

	if (character == '.')
	{
	    return 28;
	}

	return 26;
}

static void DrawText(uint32_t x, uint32_t y, const char *text, uint32_t scale, uint8_t red, uint8_t green, uint8_t blue)
{
	while (*text != '\0')
	{
	int characterIndex = GetCharacterIndex(*text++);
	const uint8_t *character = Font[characterIndex];

	    for (uint32_t row = 0; row < 7; row++)
	    {
	        for (uint32_t column = 0; column < 5; column++)
	        {
	            if ((character[row] & (1U << (4U - column))) != 0)
	            {
	                FillRectangle(x + column * scale, y + row * scale, scale, scale, red, green, blue);
	            }
	        }
	    }

	    x += 6 * scale;
	}
}

/*

* Called by the assembly entry point.
*
* The bootloader passes these values from the UEFI Graphics Output Protocol.
  */



#define EFI_MEMORY_TYPE_LOADER_CODE 1U
#define EFI_MEMORY_TYPE_LOADER_DATA 2U
#define EFI_MEMORY_TYPE_BOOT_SERVICES_CODE 3U
#define EFI_MEMORY_TYPE_BOOT_SERVICES_DATA 4U
#define EFI_MEMORY_TYPE_RUNTIME_SERVICES_CODE 5U
#define EFI_MEMORY_TYPE_RUNTIME_SERVICES_DATA 6U
#define EFI_MEMORY_TYPE_CONVENTIONAL 7U
#define EFI_MEMORY_TYPE_ACPI_RECLAIM 9U
#define EFI_MEMORY_TYPE_ACPI_NVS 10U
#define EFI_MEMORY_TYPE_PERSISTENT_MEMORY 14U
#define EFI_MEMORY_TYPE_UNACCEPTED 15U

#define JOLLYOS_PAGE_SIZE 4096ULL
#define JOLLYOS_BYTES_PER_MIB (1024ULL * 1024ULL)

static void SerialWriteUnsignedDecimal(uint64_t value)
{
    char digits[20];
    uint32_t count = 0;

    if (value == 0)
    {
        serial_write_char('0');
        return;
    }

    while (value != 0 && count < (uint32_t)sizeof(digits))
    {
        digits[count] = (char)('0' + (value % 10ULL));
        count++;
        value /= 10ULL;
    }

    while (count > 0)
    {
        count--;
        serial_write_char(digits[count]);
    }
}

static void SerialWriteHex64(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    serial_write_string("0x");

    for (int shift = 60; shift >= 0; shift -= 4)
    {
        serial_write_char(digits[(value >> shift) & 0x0FULL]);
    }
}

static void LogPagingInformation(void)
{
    uint64_t cr3 = __readcr3();

    serial_write_string("\nJollyOS: Paging information\n");
    serial_write_string("---------------------------\n");
    serial_write_string("Active CR3: ");
    SerialWriteHex64(cr3);
    serial_write_string("\nPML4 physical address: ");
    SerialWriteHex64(cr3 & ~0xFFFULL);
    serial_write_string("\n");
}

static void SerialWriteMemorySize(uint64_t bytes)
{
    SerialWriteUnsignedDecimal(bytes / JOLLYOS_BYTES_PER_MIB);
    serial_write_string(" MiB (");
    SerialWriteUnsignedDecimal(bytes);
    serial_write_string(" bytes)");
}

static int IsRamBackedMemoryType(uint32_t type)
{
    switch (type)
    {
        case EFI_MEMORY_TYPE_LOADER_CODE:
        case EFI_MEMORY_TYPE_LOADER_DATA:
        case EFI_MEMORY_TYPE_BOOT_SERVICES_CODE:
        case EFI_MEMORY_TYPE_BOOT_SERVICES_DATA:
        case EFI_MEMORY_TYPE_RUNTIME_SERVICES_CODE:
        case EFI_MEMORY_TYPE_RUNTIME_SERVICES_DATA:
        case EFI_MEMORY_TYPE_CONVENTIONAL:
        case EFI_MEMORY_TYPE_ACPI_RECLAIM:
        case EFI_MEMORY_TYPE_ACPI_NVS:
        case EFI_MEMORY_TYPE_PERSISTENT_MEMORY:
        case EFI_MEMORY_TYPE_UNACCEPTED:
            return 1;

        default:
            return 0;
    }
}

static void LogMemoryDiagnostics(const JOLLYOS_BOOT_INFO* bootInfo)
{
    const uint8_t* descriptorBytes = (const uint8_t*)(uintptr_t)bootInfo->MemoryMap;
    uint64_t descriptorCount = bootInfo->MemoryMapSize / bootInfo->MemoryMapDescriptorSize;
    uint64_t totalRamBytes = 0;
    uint64_t conventionalBytes = 0;
    uint64_t conventionalPages = 0;
    uint64_t index;

    serial_write_string("\nJollyOS: Memory diagnostics\n");
    serial_write_string("---------------------------\n");
    serial_write_string("UEFI memory descriptors: ");
    SerialWriteUnsignedDecimal(descriptorCount);
    serial_write_string("\n");

    for (index = 0; index < descriptorCount; index++)
    {
        const uint8_t* descriptor = descriptorBytes + (index * bootInfo->MemoryMapDescriptorSize);
        uint32_t type = *(const uint32_t*)(const void*)descriptor;
        uint64_t numberOfPages = *(const uint64_t*)(const void*)(descriptor + 24);
        uint64_t regionBytes = numberOfPages * JOLLYOS_PAGE_SIZE;

        if (IsRamBackedMemoryType(type))
        {
            totalRamBytes += regionBytes;
        }

        if (type == EFI_MEMORY_TYPE_CONVENTIONAL)
        {
            conventionalBytes += regionBytes;
            conventionalPages += numberOfPages;
        }
    }

    serial_write_string("Total RAM reported by UEFI: ");
    SerialWriteMemorySize(totalRamBytes);
    serial_write_string("\n");

    serial_write_string("UEFI conventional memory (candidate usable RAM): ");
    SerialWriteMemorySize(conventionalBytes);
    serial_write_string("\n");

    serial_write_string("UEFI conventional pages (4096 bytes each): ");
    SerialWriteUnsignedDecimal(conventionalPages);
    serial_write_string("\n");

    serial_write_string("Note: conventional memory is a firmware classification; the allocator will reserve required regions before use.\n");
}

static uint32_t ReadMemoryMapU32(const uint8_t* address)
{
    return (uint32_t)address[0] | ((uint32_t)address[1] << 8) | ((uint32_t)address[2] << 16) | ((uint32_t)address[3] << 24);
}

static uint64_t ReadMemoryMapU64(const uint8_t* address)
{
    return (uint64_t)ReadMemoryMapU32(address) | ((uint64_t)ReadMemoryMapU32(address + 4) << 32);
}


static void FillMemory(void* destination, uint8_t value, uint64_t byteCount)
{
    volatile uint8_t* bytes = (volatile uint8_t*)destination;

    for (uint64_t index = 0; index < byteCount; index++)
    {
        bytes[index] = value;
    }
}

static int GetBitmapBit(const uint8_t* bitmap, uint64_t pageIndex)
{
    return (bitmap[pageIndex >> 3] & (uint8_t)(1U << (pageIndex & 7U))) != 0;
}

static void SetBitmapBit(uint8_t* bitmap, uint64_t pageIndex, int value)
{
    uint8_t mask = (uint8_t)(1U << (pageIndex & 7U));

    if (value)
    {
        bitmap[pageIndex >> 3] |= mask;
    }
    else
    {
        bitmap[pageIndex >> 3] &= (uint8_t)~mask;
    }
}

static void SetBitmapPageRange(uint8_t* bitmap, uint64_t firstPage, uint64_t pageCount, int value)
{
    if (firstPage >= g_maximumPhysicalPage)
    {
        return;
    }

    if (pageCount > g_maximumPhysicalPage - firstPage)
    {
        pageCount = g_maximumPhysicalPage - firstPage;
    }

    for (uint64_t index = 0; index < pageCount; index++)
    {
        SetBitmapBit(bitmap, firstPage + index, value);
    }
}

static int ReservePhysicalRange(uint64_t physicalBase, uint64_t byteCount)
{
    uint64_t endAddress;
    uint64_t firstPage;
    uint64_t endPage;

    if (byteCount == 0 || physicalBase >= g_maximumPhysicalPage * JOLLYOS_PAGE_SIZE)
    {
        return 1;
    }

    if (physicalBase > UINT64_MAX - byteCount)
    {
        return 0;
    }

    endAddress = physicalBase + byteCount;
    firstPage = physicalBase / JOLLYOS_PAGE_SIZE;
    endPage = endAddress / JOLLYOS_PAGE_SIZE;

    if ((endAddress % JOLLYOS_PAGE_SIZE) != 0)
    {
        endPage++;
    }

    if (endPage > firstPage)
    {
        SetBitmapPageRange(g_pageStateBitmap, firstPage, endPage - firstPage, 1);
    }

    return 1;
}

uint64_t PhysicalPageAllocate(void)
{
    return PhysicalPageAllocateContiguous(1);
}

uint64_t PhysicalPageAllocateContiguous(uint64_t pageCount)
{
    uint64_t candidateStart = 1;

    if (!g_physicalMemoryReady || pageCount == 0 || pageCount > g_freePageCount)
    {
        return 0;
    }

    if (pageCount > g_maximumPhysicalPage)
    {
        return 0;
    }

    while (candidateStart < g_maximumPhysicalPage)
    {
        uint64_t runLength = 0;

        if (pageCount > g_maximumPhysicalPage - candidateStart)
        {
            return 0;
        }

        while (runLength < pageCount)
        {
            uint64_t pageIndex = candidateStart + runLength;

            if (!GetBitmapBit(g_managedPageBitmap, pageIndex) || GetBitmapBit(g_pageStateBitmap, pageIndex))
            {
                break;
            }

            runLength++;
        }

        if (runLength == pageCount)
        {
            for (uint64_t index = 0; index < pageCount; index++)
            {
                uint64_t pageIndex = candidateStart + index;

                SetBitmapBit(g_pageStateBitmap, pageIndex, 1);
                SetBitmapBit(g_allocatorOwnedBitmap, pageIndex, 1);
            }

            g_freePageCount -= pageCount;
            g_allocatorOwnedPageCount += pageCount;

            return candidateStart * JOLLYOS_PAGE_SIZE;
        }

        candidateStart += runLength + 1;
    }

    return 0;
}

int PhysicalPageFree(uint64_t physicalAddress)
{
    return PhysicalPageFreeContiguous(physicalAddress, 1);
}

int PhysicalPageFreeContiguous(uint64_t physicalAddress, uint64_t pageCount)
{
    uint64_t firstPage;

    if (!g_physicalMemoryReady || physicalAddress == 0 || pageCount == 0 || (physicalAddress & (JOLLYOS_PAGE_SIZE - 1ULL)) != 0)
    {
        return 0;
    }

    firstPage = physicalAddress / JOLLYOS_PAGE_SIZE;

    if (firstPage >= g_maximumPhysicalPage || pageCount > g_maximumPhysicalPage - firstPage)
    {
        return 0;
    }

    /*
     * Validate the entire range before changing anything.
     * This prevents a partial free if one page is invalid.
     */
    for (uint64_t index = 0; index < pageCount; index++)
    {
        uint64_t pageIndex = firstPage + index;

        if (!GetBitmapBit(g_managedPageBitmap, pageIndex) || !GetBitmapBit(g_allocatorOwnedBitmap, pageIndex) || !GetBitmapBit(g_pageStateBitmap, pageIndex))
        {
            return 0;
        }
    }

    for (uint64_t index = 0; index < pageCount; index++)
    {
        uint64_t pageIndex = firstPage + index;

        SetBitmapBit(g_allocatorOwnedBitmap, pageIndex, 0);
        SetBitmapBit(g_pageStateBitmap, pageIndex, 0);
    }

    g_freePageCount += pageCount;
    g_allocatorOwnedPageCount -= pageCount;

    return 1;
}

int PhysicalMemoryGetStats(JOLLYOS_PHYSICAL_MEMORY_STATS* stats)
{
    if (!g_physicalMemoryReady || stats == NULL)
    {
        return 0;
    }

    stats->ManagedPages = g_managedPageCount;
    stats->FreePages = g_freePageCount;
    stats->ReservedPages = g_managedPageCount - g_freePageCount - g_allocatorOwnedPageCount;
    stats->AllocatedPages = g_allocatorOwnedPageCount;
    return 1;
}

static void LogPhysicalMemoryStatistics(void)
{
    uint64_t reservedPageCount = g_managedPageCount - g_freePageCount - g_allocatorOwnedPageCount;

    serial_write_string("\nJollyOS: Physical memory manager\n");
    serial_write_string("-------------------------------\n");
    serial_write_string("Managed conventional pages: ");
    SerialWriteUnsignedDecimal(g_managedPageCount);
    serial_write_string("\nFree pages: ");
    SerialWriteUnsignedDecimal(g_freePageCount);
    serial_write_string(" (");
    SerialWriteUnsignedDecimal(g_freePageCount * JOLLYOS_PAGE_SIZE / JOLLYOS_BYTES_PER_MIB);
    serial_write_string(" MiB)\nReserved pages: ");
    SerialWriteUnsignedDecimal(reservedPageCount);
    serial_write_string("\nPages currently allocated by the allocator: ");
    SerialWriteUnsignedDecimal(g_allocatorOwnedPageCount);
    serial_write_string("\nPage size: 4096 bytes\n");
}

int PhysicalMemoryInitialize(const JOLLYOS_BOOT_INFO* bootInfo)
{
    const uint8_t* descriptorBytes = (const uint8_t*)(uintptr_t)bootInfo->MemoryMap;
    uint64_t descriptorCount = bootInfo->MemoryMapSize / bootInfo->MemoryMapDescriptorSize;
    uint64_t maximumPage = 0;
    uint64_t bitmapBytes;
    uint64_t bitmapStorageBytes;
    uint64_t bitmapStoragePages;
    uint64_t bitmapPhysicalAddress = 0;
    uint64_t bitmapPageCount = 0;

    for (uint64_t index = 0; index < descriptorCount; index++)
    {
        const uint8_t* descriptor = descriptorBytes + (index * bootInfo->MemoryMapDescriptorSize);
        uint32_t type = ReadMemoryMapU32(descriptor);
        uint64_t physicalStart = ReadMemoryMapU64(descriptor + 8);
        uint64_t numberOfPages = ReadMemoryMapU64(descriptor + 24);
        uint64_t endPage;

        if (type != EFI_MEMORY_TYPE_CONVENTIONAL || numberOfPages == 0)
        {
            continue;
        }

        if ((physicalStart & (JOLLYOS_PAGE_SIZE - 1ULL)) != 0 || numberOfPages > UINT64_MAX - (physicalStart / JOLLYOS_PAGE_SIZE))
        {
            serial_write_string("ERROR: Invalid conventional memory descriptor.\n");
            return 0;
        }

        endPage = (physicalStart / JOLLYOS_PAGE_SIZE) + numberOfPages;

        if (endPage > maximumPage)
        {
            maximumPage = endPage;
        }
    }

    if (maximumPage < 2 || maximumPage > UINT64_MAX - 7ULL)
    {
        serial_write_string("ERROR: No usable conventional memory range was found.\n");
        return 0;
    }

    bitmapBytes = (maximumPage + 7ULL) / 8ULL;

    if (bitmapBytes > UINT64_MAX / 3ULL)
    {
        serial_write_string("ERROR: Physical memory bitmap size overflowed.\n");
        return 0;
    }

    bitmapStorageBytes = bitmapBytes * 3ULL;

    if (bitmapStorageBytes > UINT64_MAX - (JOLLYOS_PAGE_SIZE - 1ULL))
    {
        serial_write_string("ERROR: Physical memory bitmap storage size overflowed.\n");
        return 0;
    }

    bitmapStoragePages = (bitmapStorageBytes + JOLLYOS_PAGE_SIZE - 1ULL) / JOLLYOS_PAGE_SIZE;

    for (uint64_t index = 0; index < descriptorCount; index++)
    {
        const uint8_t* descriptor = descriptorBytes + (index * bootInfo->MemoryMapDescriptorSize);
        uint32_t type = ReadMemoryMapU32(descriptor);
        uint64_t physicalStart = ReadMemoryMapU64(descriptor + 8);
        uint64_t numberOfPages = ReadMemoryMapU64(descriptor + 24);

        if (type == EFI_MEMORY_TYPE_CONVENTIONAL && physicalStart >= 0x100000ULL && numberOfPages >= bitmapStoragePages)
        {
            bitmapPhysicalAddress = physicalStart;
            bitmapPageCount = bitmapStoragePages;
            break;
        }
    }

    if (bitmapPageCount == 0)
    {
        serial_write_string("ERROR: No conventional memory range can hold the physical page bitmaps.\n");
        return 0;
    }

    g_maximumPhysicalPage = maximumPage;
    g_pageStateBitmap = (uint8_t*)(uintptr_t)bitmapPhysicalAddress;
    g_managedPageBitmap = g_pageStateBitmap + bitmapBytes;
    g_allocatorOwnedBitmap = g_managedPageBitmap + bitmapBytes;

    FillMemory(g_pageStateBitmap, 0xFFU, bitmapBytes);
    FillMemory(g_managedPageBitmap, 0U, bitmapBytes);
    FillMemory(g_allocatorOwnedBitmap, 0U, bitmapBytes);

    for (uint64_t index = 0; index < descriptorCount; index++)
    {
        const uint8_t* descriptor = descriptorBytes + (index * bootInfo->MemoryMapDescriptorSize);
        uint32_t type = ReadMemoryMapU32(descriptor);
        uint64_t physicalStart = ReadMemoryMapU64(descriptor + 8);
        uint64_t numberOfPages = ReadMemoryMapU64(descriptor + 24);
        uint64_t firstPage;

        if (type != EFI_MEMORY_TYPE_CONVENTIONAL || numberOfPages == 0)
        {
            continue;
        }

        firstPage = physicalStart / JOLLYOS_PAGE_SIZE;
        SetBitmapPageRange(g_pageStateBitmap, firstPage, numberOfPages, 0);
        SetBitmapPageRange(g_managedPageBitmap, firstPage, numberOfPages, 1);
    }

    /* Physical page zero is never returned as an allocation. */
    SetBitmapBit(g_pageStateBitmap, 0, 1);

    if (!ReservePhysicalRange(0, 0x100000ULL) || !ReservePhysicalRange(bootInfo->KernelBase, bootInfo->KernelSize) || !ReservePhysicalRange((uint64_t)(uintptr_t)bootInfo, sizeof(*bootInfo)) || !ReservePhysicalRange(bootInfo->MemoryMap, bootInfo->MemoryMapSize))
    {
        serial_write_string("ERROR: A required physical memory reservation overflowed.\n");
        return 0;
    }

    if (bootInfo->PixelsPerScanLine != 0 && bootInfo->Height != 0)
    {
        uint64_t framebufferPixels = (uint64_t)bootInfo->PixelsPerScanLine * (uint64_t)bootInfo->Height;

        if (framebufferPixels > UINT64_MAX / 4ULL)
        {
            serial_write_string("ERROR: Framebuffer memory range overflowed.\n");
            return 0;
        }

        if (!ReservePhysicalRange(bootInfo->Framebuffer, framebufferPixels * 4ULL))
        {
            serial_write_string("ERROR: Framebuffer memory reservation overflowed.\n");
            return 0;
        }
    }

    if (!ReservePhysicalRange(bitmapPhysicalAddress, bitmapPageCount * JOLLYOS_PAGE_SIZE))
    {
        serial_write_string("ERROR: Allocator bitmap reservation overflowed.\n");
        return 0;
    }

    g_managedPageCount = 0;
    g_freePageCount = 0;
    g_allocatorOwnedPageCount = 0;

    for (uint64_t pageIndex = 0; pageIndex < g_maximumPhysicalPage; pageIndex++)
    {
        if (GetBitmapBit(g_managedPageBitmap, pageIndex))
        {
            g_managedPageCount++;

            if (!GetBitmapBit(g_pageStateBitmap, pageIndex))
            {
                g_freePageCount++;
            }
        }
    }

    g_physicalMemoryReady = 1;
    return 1;
}


static int PhysicalMemorySelfTest(void)
{
    JOLLYOS_PHYSICAL_MEMORY_STATS initialStats;
    JOLLYOS_PHYSICAL_MEMORY_STATS currentStats;
    uint64_t firstPage;
    uint64_t mixedRange;
    uint64_t invalidAddress = UINT64_MAX & ~(JOLLYOS_PAGE_SIZE - 1ULL);

    if (!PhysicalMemoryGetStats(&initialStats))
    {
        serial_write_string("ERROR: Cannot read allocator statistics before testing.\n");
        return 0;
    }

    /* Invalid allocation requests must fail without changing allocator state. */
    if (PhysicalPageAllocateContiguous(0) != 0 || PhysicalPageAllocateContiguous(UINT64_MAX) != 0)
    {
        serial_write_string("ERROR: Allocator accepted an invalid allocation request.\n");
        return 0;
    }

    if (PhysicalPageFree(0) || PhysicalPageFree(1) || PhysicalPageFree(invalidAddress) || PhysicalPageFreeContiguous(0, 1) || PhysicalPageFreeContiguous(1, 2) || PhysicalPageFreeContiguous(invalidAddress, 1))
    {
        serial_write_string("ERROR: Allocator accepted an invalid free request.\n");
        return 0;
    }

    if (!PhysicalMemoryGetStats(&currentStats) || currentStats.FreePages != initialStats.FreePages || currentStats.ReservedPages != initialStats.ReservedPages || currentStats.AllocatedPages != initialStats.AllocatedPages)
    {
        serial_write_string("ERROR: Invalid requests changed allocator statistics.\n");
        return 0;
    }

    /* Allocate three adjacent physical pages and verify their addresses. */
    firstPage = PhysicalPageAllocateContiguous(3);

    if (firstPage == 0 || (firstPage & (JOLLYOS_PAGE_SIZE - 1ULL)) != 0)
    {
        serial_write_string("ERROR: Contiguous page allocation failed.\n");
        return 0;
    }

    if (!PhysicalMemoryGetStats(&currentStats) || currentStats.AllocatedPages != initialStats.AllocatedPages + 3 || currentStats.FreePages + 3 != initialStats.FreePages)
    {
        serial_write_string("ERROR: Contiguous allocation statistics are incorrect.\n");
        return 0;
    }

    if (!PhysicalPageFreeContiguous(firstPage, 3))
    {
        serial_write_string("ERROR: Contiguous page free failed.\n");
        return 0;
    }

    if (PhysicalPageFreeContiguous(firstPage, 3))
    {
        serial_write_string("ERROR: Allocator accepted a duplicate contiguous free.\n");
        return 0;
    }

    /* Verify that a partly invalid range is rejected without partially freeing it. */
    mixedRange = PhysicalPageAllocateContiguous(2);

    if (mixedRange == 0)
    {
        serial_write_string("ERROR: Could not allocate pages for the partial-free test.\n");
        return 0;
    }

    if (!PhysicalPageFree(mixedRange))
    {
        serial_write_string("ERROR: Could not free the first test page.\n");
        PhysicalPageFreeContiguous(mixedRange, 2);
        return 0;
    }

    if (PhysicalPageFreeContiguous(mixedRange, 2))
    {
        serial_write_string("ERROR: Allocator accepted a partly freed range.\n");
        return 0;
    }

    if (!PhysicalMemoryGetStats(&currentStats) || currentStats.AllocatedPages != initialStats.AllocatedPages + 1)
    {
        serial_write_string("ERROR: Partial-free rejection corrupted allocator statistics.\n");
        return 0;
    }

    if (!PhysicalPageFree(mixedRange + JOLLYOS_PAGE_SIZE))
    {
        serial_write_string("ERROR: Could not free the remaining test page.\n");
        return 0;
    }

    if (!PhysicalMemoryGetStats(&currentStats) || currentStats.ManagedPages != initialStats.ManagedPages || currentStats.FreePages != initialStats.FreePages || currentStats.ReservedPages != initialStats.ReservedPages || currentStats.AllocatedPages != initialStats.AllocatedPages)
    {
        serial_write_string("ERROR: Allocator statistics did not return to their initial values.\n");
        return 0;
    }

    serial_write_string("JollyOS: Physical memory allocation/free validation passed.\n");
    serial_write_string("JollyOS: Contiguous allocation self-test passed.\n");
    serial_write_string("JollyOS: Invalid request and statistics self-tests passed.\n");

    return 1;
}

void kernel_main(const JOLLYOS_BOOT_INFO* bootInfo)
{
	serial_initialize();
	serial_write_string("\nJollyOS: Kernel entry reached.\n");

	if (bootInfo == NULL)
	{
		serial_write_string("ERROR: Boot information pointer is null.\n");
		for (;;)
		{
			__halt();
		}
	}

	serial_write_string("JollyOS: Boot information pointer is valid.\n");

	if (bootInfo->Version != JOLLYOS_BOOT_INFO_VERSION)
	{
		serial_write_string("ERROR: Unsupported boot information version.\n");
		for (;;)
		{
			__halt();
		}
	}

	if (bootInfo->Size < sizeof(JOLLYOS_BOOT_INFO))
	{
		serial_write_string("ERROR: Boot information structure is too small.\n");
		for (;;)
		{
			__halt();
		}
	}

	serial_write_string("JollyOS: Boot information version and size validated.\n");

	if (bootInfo->Framebuffer == 0 || bootInfo->Width == 0 || bootInfo->Height == 0 || bootInfo->PixelsPerScanLine < bootInfo->Width ||	(bootInfo->PixelFormat != 0 && bootInfo->PixelFormat != 1))
	{
		serial_write_string("ERROR: Invalid framebuffer information.\n");
		for (;;)
		{
			__halt();
		}
	}

	if (bootInfo->MemoryMap == 0 ||	bootInfo->MemoryMapSize == 0 ||	bootInfo->MemoryMapDescriptorSize < 40 || bootInfo->MemoryMapDescriptorSize > bootInfo->MemoryMapSize || bootInfo->MemoryMapSize % bootInfo->MemoryMapDescriptorSize != 0)
	{
		serial_write_string("ERROR: Invalid UEFI memory map information.\n");
		for (;;)
		{
			__halt();
		}
	}

	if (bootInfo->KernelBase == 0 || bootInfo->KernelSize == 0)
	{
		serial_write_string("ERROR: Invalid kernel image information.\n");
		for (;;)
		{
			__halt();
		}
	}

	serial_write_string("JollyOS: All boot information validated.\n");
	LogMemoryDiagnostics(bootInfo);

    if (!PhysicalMemoryInitialize(bootInfo))
    {
        serial_write_string("FATAL: Physical memory manager initialisation failed.\n");

        for (;;)
        {
            __halt();
        }
    }

    if (!PhysicalMemorySelfTest())
    {
        serial_write_string("FATAL: Physical memory manager self-test failed.\n");

        for (;;)
        {
            __halt();
        }
    }

    LogPagingInformation();
    
    if (!VirtualMemoryInitialize())
    {
        serial_write_string("FATAL: Virtual memory initialisation failed.\n");

        for (;;)
        {
            __halt();
        }
    }

    if (!VirtualMemorySelfTest())
    {
        serial_write_string("FATAL: Virtual memory self-test failed.\n");

        for (;;)
        {
            __halt();
        }
    }
    
    LogPhysicalMemoryStatistics();
	serial_write_string("JollyOS: Initialising framebuffer.\n");

	g_framebuffer = (volatile uint8_t*)(uintptr_t)bootInfo->Framebuffer;
	g_width = bootInfo->Width;
	g_height = bootInfo->Height;
	g_pixelsPerScanLine = bootInfo->PixelsPerScanLine;
	g_pixelFormat = bootInfo->PixelFormat;

	FillRectangle(0, 0, g_width, g_height, 8, 12, 20);
	FillRectangle(0, 0, g_width, 12, 30, 110, 220);
	DrawText(40, 50, "WELCOME TO JOLLYOS!", 4, 255, 255, 255);
	DrawText(40, 100, "BOOTED FROM OUR OWN UEFI LOADER.", 2, 220, 230, 245);

	serial_write_string("JollyOS: Welcome screen drawn successfully.\n");
	serial_write_string("JollyOS: Boot initialisation complete.\n");

	for (;;)
	{
		__halt();
	}
}