#include <stdint.h>
#include <stddef.h>
#include <intrin.h>

#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include "serial.h"

#define VIRTUAL_PAGE_PRESENT 0x001ULL
#define VIRTUAL_PAGE_WRITABLE 0x002ULL
#define VIRTUAL_PAGE_USER 0x004ULL
#define VIRTUAL_PAGE_LARGE 0x080ULL
#define VIRTUAL_PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define VIRTUAL_PAGE_2MB_ADDRESS_MASK 0x000FFFFFFFE00000ULL
#define VIRTUAL_PAGE_1GB_ADDRESS_MASK 0x000FFFFFC0000000ULL
#define VIRTUAL_MEMORY_EFER_MSR 0xC0000080U
#define VIRTUAL_MEMORY_EFER_NXE 0x00000800ULL
#define VIRTUAL_MEMORY_EXTENDED_CPUID_MAX 0x80000000U
#define VIRTUAL_MEMORY_EXTENDED_FEATURES 0x80000001U
#define VIRTUAL_MEMORY_NX_FEATURE (1U << 20)

static int g_virtualMemoryInitialized;
static int g_virtualMemoryNoExecuteSupported;

static int VirtualAddressIsCanonical(uint64_t address)
{
    uint64_t upperBits = address >> 48;
    uint64_t signBit = (address >> 47) & 1ULL;

    if (signBit == 0)
    {
        return upperBits == 0;
    }

    return upperBits == 0xFFFFULL;
}

static uint64_t* VirtualMemoryGetTable(uint64_t physicalAddress)
{
    physicalAddress &= VIRTUAL_PAGE_ADDRESS_MASK;
    return (uint64_t*)(uintptr_t)physicalAddress;
}

static uint64_t VirtualMemoryGetCurrentPml4Address(void)
{
    return __readcr3() & VIRTUAL_PAGE_ADDRESS_MASK;
}

static uint64_t* VirtualMemoryGetCurrentPml4(void)
{
    return VirtualMemoryGetTable(VirtualMemoryGetCurrentPml4Address());
}

static uint64_t VirtualMemoryAllocateTable(void)
{
    uint64_t physicalAddress = PhysicalPageAllocate();

    if (physicalAddress == 0)
    {
        return 0;
    }

    uint64_t* table = VirtualMemoryGetTable(physicalAddress);

    for (uint32_t index = 0; index < 512; index++)
    {
        table[index] = 0;
    }

    return physicalAddress;
}

int VirtualMemoryInitialize(void)
{
    int cpuInfo[4];

    if (g_virtualMemoryInitialized)
    {
        return 1;
    }

    if (VirtualMemoryGetCurrentPml4Address() == 0)
    {
        serial_write_string("ERROR: Active PML4 address is zero.\n");
        return 0;
    }

    __cpuid(cpuInfo, (int)VIRTUAL_MEMORY_EXTENDED_CPUID_MAX);

    if ((uint32_t)cpuInfo[0] >= VIRTUAL_MEMORY_EXTENDED_FEATURES)
    {
        __cpuid(cpuInfo, (int)VIRTUAL_MEMORY_EXTENDED_FEATURES);

        if (((uint32_t)cpuInfo[3] & VIRTUAL_MEMORY_NX_FEATURE) != 0)
        {
            uint64_t efer = __readmsr(VIRTUAL_MEMORY_EFER_MSR);

            if ((efer & VIRTUAL_MEMORY_EFER_NXE) == 0)
            {
                __writemsr(VIRTUAL_MEMORY_EFER_MSR, efer | VIRTUAL_MEMORY_EFER_NXE);
            }

            g_virtualMemoryNoExecuteSupported = 1;
            serial_write_string("JollyOS: CPU NX page protection enabled.\n");
        }
        else
        {
            serial_write_string("JollyOS: CPU does not support NX page protection.\n");
        }
    }

    g_virtualMemoryInitialized = 1;
    return 1;
}

int VirtualMemorySupportsNoExecute(void)
{
    return g_virtualMemoryNoExecuteSupported;
}

int VirtualMemoryTranslate(uint64_t virtualAddress, uint64_t* physicalAddress)
{
    uint64_t* pml4;
    uint64_t* pdpt;
    uint64_t* pd;
    uint64_t* pt;
    uint64_t entry;

    if (!g_virtualMemoryInitialized || physicalAddress == NULL)
    {
        return 0;
    }

    if (!VirtualAddressIsCanonical(virtualAddress))
    {
        return 0;
    }

    pml4 = VirtualMemoryGetCurrentPml4();

    entry = pml4[(virtualAddress >> 39) & 0x1FFULL];

    if ((entry & VIRTUAL_PAGE_PRESENT) == 0)
    {
        return 0;
    }

    pdpt = VirtualMemoryGetTable(entry);

    entry = pdpt[(virtualAddress >> 30) & 0x1FFULL];

    if ((entry & VIRTUAL_PAGE_PRESENT) == 0)
    {
        return 0;
    }

    if ((entry & VIRTUAL_PAGE_LARGE) != 0)
    {
        *physicalAddress = (entry & VIRTUAL_PAGE_1GB_ADDRESS_MASK) | (virtualAddress & 0x3FFFFFFFULL);
        return 1;
    }

    pd = VirtualMemoryGetTable(entry);

    entry = pd[(virtualAddress >> 21) & 0x1FFULL];

    if ((entry & VIRTUAL_PAGE_PRESENT) == 0)
    {
        return 0;
    }

    if ((entry & VIRTUAL_PAGE_LARGE) != 0)
    {
        *physicalAddress = (entry & VIRTUAL_PAGE_2MB_ADDRESS_MASK) | (virtualAddress & 0x1FFFFFULL);
        return 1;
    }

    pt = VirtualMemoryGetTable(entry);

    entry = pt[(virtualAddress >> 12) & 0x1FFULL];

    if ((entry & VIRTUAL_PAGE_PRESENT) == 0)
    {
        return 0;
    }

    *physicalAddress = (entry & VIRTUAL_PAGE_ADDRESS_MASK) | (virtualAddress & (VIRTUAL_MEMORY_PAGE_SIZE - 1ULL));

    return 1;
}

int VirtualMemoryGetPageFlags(uint64_t virtualAddress, uint64_t* flags)
{
    uint64_t* table;
    uint64_t entry;
    uint64_t effectiveFlags = VIRTUAL_MEMORY_FLAG_WRITABLE | VIRTUAL_MEMORY_FLAG_USER;
    uint64_t leafFlags;

    if (!g_virtualMemoryInitialized || flags == NULL)
    {
        return 0;
    }

    if (!VirtualAddressIsCanonical(virtualAddress))
    {
        return 0;
    }

    table = VirtualMemoryGetCurrentPml4();

    for (uint32_t level = 0; level < 4; level++)
    {
        uint32_t shift = 39U - (level * 9U);
        uint64_t index = (virtualAddress >> shift) & 0x1FFULL;

        entry = table[index];

        if ((entry & VIRTUAL_PAGE_PRESENT) == 0)
        {
            return 0;
        }

        if ((entry & VIRTUAL_PAGE_WRITABLE) == 0)
        {
            effectiveFlags &= ~VIRTUAL_MEMORY_FLAG_WRITABLE;
        }

        if ((entry & VIRTUAL_PAGE_USER) == 0)
        {
            effectiveFlags &= ~VIRTUAL_MEMORY_FLAG_USER;
        }

        if (g_virtualMemoryNoExecuteSupported && (entry & VIRTUAL_MEMORY_FLAG_NO_EXECUTE) != 0)
        {
            effectiveFlags |= VIRTUAL_MEMORY_FLAG_NO_EXECUTE;
        }

        if (level == 3 || (level > 0 && (entry & VIRTUAL_PAGE_LARGE) != 0))
        {
            leafFlags = entry & (VIRTUAL_MEMORY_FLAG_WRITE_THROUGH | VIRTUAL_MEMORY_FLAG_CACHE_DISABLE | VIRTUAL_MEMORY_FLAG_GLOBAL);

            effectiveFlags |= leafFlags;

            *flags = effectiveFlags;
            return 1;
        }

        table = VirtualMemoryGetTable(entry);
    }

    return 0;
}

int VirtualMemoryMapPage(uint64_t virtualAddress, uint64_t physicalAddress, uint64_t flags)
{
    const uint64_t allowedFlags = VIRTUAL_MEMORY_FLAG_WRITABLE | VIRTUAL_MEMORY_FLAG_USER | VIRTUAL_MEMORY_FLAG_WRITE_THROUGH | VIRTUAL_MEMORY_FLAG_CACHE_DISABLE | VIRTUAL_MEMORY_FLAG_GLOBAL | VIRTUAL_MEMORY_FLAG_NO_EXECUTE;

    uint64_t* table;
    uint64_t entry;
    uint64_t tablePhysicalAddress;
    uint64_t intermediateFlags;
    uint64_t* changedParentTables[3];
    uint64_t changedIndices[3];
    uint64_t oldEntries[3];
    uint64_t allocatedTables[3];
    uint32_t changedCount = 0;
    uint32_t allocatedCount = 0;

    if (!g_virtualMemoryInitialized)
    {
        return 0;
    }

    if (!VirtualAddressIsCanonical(virtualAddress))
    {
        return 0;
    }

    if ((virtualAddress & (VIRTUAL_MEMORY_PAGE_SIZE - 1ULL)) != 0 || (physicalAddress & (VIRTUAL_MEMORY_PAGE_SIZE - 1ULL)) != 0)
    {
        return 0;
    }

    if ((physicalAddress & ~VIRTUAL_PAGE_ADDRESS_MASK) != 0)
    {
        return 0;
    }

    if ((flags & ~allowedFlags) != 0)
    {
        return 0;
    }

    if ((flags & VIRTUAL_MEMORY_FLAG_NO_EXECUTE) != 0 && !g_virtualMemoryNoExecuteSupported)
    {
        return 0;
    }

    table = VirtualMemoryGetCurrentPml4();
    intermediateFlags = VIRTUAL_PAGE_PRESENT | VIRTUAL_PAGE_WRITABLE;

    if ((flags & VIRTUAL_MEMORY_FLAG_USER) != 0)
    {
        intermediateFlags |= VIRTUAL_PAGE_USER;
    }

    for (uint32_t level = 0; level < 3; level++)
    {
        uint32_t shift = 39U - (level * 9U);
        uint64_t index = (virtualAddress >> shift) & 0x1FFULL;

        entry = table[index];

        if ((entry & VIRTUAL_PAGE_PRESENT) == 0)
        {
            tablePhysicalAddress = VirtualMemoryAllocateTable();

            if (tablePhysicalAddress == 0)
            {
                goto rollback;
            }

            allocatedTables[allocatedCount++] = tablePhysicalAddress;

            changedParentTables[changedCount] = table;
            changedIndices[changedCount] = index;
            oldEntries[changedCount] = entry;
            changedCount++;

            table[index] = tablePhysicalAddress | intermediateFlags;
            entry = table[index];
        }
        else
        {
            if (level != 0 && (entry & VIRTUAL_PAGE_LARGE) != 0)
            {
                goto rollback;
            }

            uint64_t updatedEntry = entry;

            if ((flags & VIRTUAL_MEMORY_FLAG_WRITABLE) != 0)
            {
                updatedEntry |= VIRTUAL_PAGE_WRITABLE;
            }

            if ((flags & VIRTUAL_MEMORY_FLAG_USER) != 0)
            {
                updatedEntry |= VIRTUAL_PAGE_USER;
            }

            if (updatedEntry != entry)
            {
                changedParentTables[changedCount] = table;
                changedIndices[changedCount] = index;
                oldEntries[changedCount] = entry;
                changedCount++;

                table[index] = updatedEntry;
                entry = updatedEntry;
            }
        }

        table = VirtualMemoryGetTable(entry);
    }

    {
        uint64_t index = (virtualAddress >> 12) & 0x1FFULL;

        if ((table[index] & VIRTUAL_PAGE_PRESENT) != 0)
        {
            goto rollback;
        }

        table[index] = physicalAddress | VIRTUAL_PAGE_PRESENT | (flags & allowedFlags);
    }

    __invlpg((void*)(uintptr_t)virtualAddress);
    return 1;

rollback:
    while (changedCount > 0)
    {
        changedCount--;

        changedParentTables[changedCount][changedIndices[changedCount]] = oldEntries[changedCount];
    }

    while (allocatedCount > 0)
    {
        allocatedCount--;

        if (!PhysicalPageFree(allocatedTables[allocatedCount]))
        {
            serial_write_string("ERROR: Failed to free a rolled-back page table.\n");
        }
    }

    return 0;
}

static int VirtualMemoryTableIsEmpty(uint64_t* table)
{
    for (uint32_t index = 0; index < 512; index++)
    {
        if (table[index] != 0)
        {
            return 0;
        }
    }

    return 1;
}

int VirtualMemoryUnmapPage(uint64_t virtualAddress)
{
    uint64_t* table;
    uint64_t entry;
    uint64_t parentIndices[3];
    uint64_t childPhysicalAddresses[3];
    uint64_t* parentTables[3];

    if (!g_virtualMemoryInitialized)
    {
        return 0;
    }

    if (!VirtualAddressIsCanonical(virtualAddress))
    {
        return 0;
    }

    if ((virtualAddress & (VIRTUAL_MEMORY_PAGE_SIZE - 1ULL)) != 0)
    {
        return 0;
    }

    table = VirtualMemoryGetCurrentPml4();

    for (uint32_t level = 0; level < 3; level++)
    {
        uint32_t shift = 39U - (level * 9U);
        uint64_t index = (virtualAddress >> shift) & 0x1FFULL;

        entry = table[index];

        if ((entry & VIRTUAL_PAGE_PRESENT) == 0)
        {
            return 0;
        }

        if ((entry & VIRTUAL_PAGE_LARGE) != 0)
        {
            return 0;
        }

        parentTables[level] = table;
        parentIndices[level] = index;
        childPhysicalAddresses[level] = entry & VIRTUAL_PAGE_ADDRESS_MASK;

        table = VirtualMemoryGetTable(entry);
    }

    {
        uint64_t index = (virtualAddress >> 12) & 0x1FFULL;

        if ((table[index] & VIRTUAL_PAGE_PRESENT) == 0)
        {
            return 0;
        }

        table[index] = 0;
    }

    __invlpg((void*)(uintptr_t)virtualAddress);

    for (int level = 2; level >= 0; level--)
    {
        uint64_t* childTable = VirtualMemoryGetTable(childPhysicalAddresses[level]);

        if (!VirtualMemoryTableIsEmpty(childTable))
        {
            break;
        }

        parentTables[level][parentIndices[level]] = 0;

        PhysicalPageFree(childPhysicalAddresses[level]);
    }

    __invlpg((void*)(uintptr_t)virtualAddress);

    return 1;
}

int VirtualMemorySelfTest(void)
{
    const uint64_t testVirtualAddress = 0x0000400000000000ULL;
    const uint64_t testPattern = 0x4A4F4C4C594F5331ULL;
    const uint64_t invalidVirtualAddress = 0x0000800000000000ULL;
    uint64_t testPhysicalAddress;
    uint64_t secondPhysicalAddress;
    uint64_t translatedAddress;
    uint64_t mappedFlags;
    uint64_t userFlags;
    uint64_t* physicalPointer;
    volatile uint64_t* virtualPointer;
    uint64_t flags = VIRTUAL_MEMORY_FLAG_WRITABLE;

    if (!g_virtualMemoryInitialized)
    {
        return 0;
    }

    if (VirtualMemoryTranslate(testVirtualAddress, &translatedAddress))
    {
        serial_write_string("ERROR: Virtual-memory test address is already mapped.\n");
        return 0;
    }

    testPhysicalAddress = PhysicalPageAllocate();

    if (testPhysicalAddress == 0)
    {
        serial_write_string("ERROR: Could not allocate virtual-memory test page.\n");
        return 0;
    }

    secondPhysicalAddress = PhysicalPageAllocate();

    if (secondPhysicalAddress == 0)
    {
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Could not allocate second virtual-memory test page.\n");
        return 0;
    }

    physicalPointer = (uint64_t*)(uintptr_t)testPhysicalAddress;
    *physicalPointer = testPattern;

    if (g_virtualMemoryNoExecuteSupported)
    {
        flags |= VIRTUAL_MEMORY_FLAG_NO_EXECUTE;
    }

    if (!VirtualMemoryMapPage(testVirtualAddress, testPhysicalAddress, flags))
    {
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Virtual-memory test mapping failed.\n");
        return 0;
    }

    if (!VirtualMemoryGetPageFlags(testVirtualAddress, &mappedFlags))
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Could not query virtual-memory page flags.\n");
        return 0;
    }

    if ((mappedFlags & VIRTUAL_MEMORY_FLAG_WRITABLE) == 0)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Writable page permission was not reported.\n");
        return 0;
    }

    if ((mappedFlags & VIRTUAL_MEMORY_FLAG_USER) != 0)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Kernel page was incorrectly reported as user-accessible.\n");
        return 0;
    }

    if (g_virtualMemoryNoExecuteSupported && (mappedFlags & VIRTUAL_MEMORY_FLAG_NO_EXECUTE) == 0)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: NX page permission was not reported.\n");
        return 0;
    }

    if (VirtualMemoryMapPage(testVirtualAddress, secondPhysicalAddress, flags))
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Duplicate virtual-memory mapping was accepted.\n");
        return 0;
    }

    if (!VirtualMemoryTranslate(testVirtualAddress, &translatedAddress) || translatedAddress != testPhysicalAddress)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Duplicate mapping damaged the original mapping.\n");
        return 0;
    }

    if (VirtualMemoryMapPage(testVirtualAddress + 1ULL, secondPhysicalAddress, flags))
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Unaligned virtual address was accepted.\n");
        return 0;
    }

    if (VirtualMemoryMapPage(invalidVirtualAddress, secondPhysicalAddress, flags))
    {
        VirtualMemoryUnmapPage(invalidVirtualAddress);
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Non-canonical virtual address was accepted.\n");
        return 0;
    }

    if (!VirtualMemoryTranslate(testVirtualAddress, &translatedAddress) || translatedAddress != testPhysicalAddress)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Virtual-memory translation test failed.\n");
        return 0;
    }

    virtualPointer = (volatile uint64_t*)(uintptr_t)testVirtualAddress;

    if (*virtualPointer != testPattern)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Virtual-memory read test failed.\n");
        return 0;
    }

    *virtualPointer = ~testPattern;

    if (*physicalPointer != ~testPattern)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Virtual-memory write test failed.\n");
        return 0;
    }

    if (!VirtualMemoryUnmapPage(testVirtualAddress))
    {
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Virtual-memory unmap test failed.\n");
        return 0;
    }

    if (VirtualMemoryTranslate(testVirtualAddress, &translatedAddress))
    {
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Unmapped virtual address still translates.\n");
        return 0;
    }

    if (VirtualMemoryGetPageFlags(testVirtualAddress, &mappedFlags))
    {
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Unmapped virtual address still has page flags.\n");
        return 0;
    }

    if (VirtualMemoryUnmapPage(testVirtualAddress))
    {
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Unmapping an unmapped address succeeded.\n");
        return 0;
    }

    userFlags = flags | VIRTUAL_MEMORY_FLAG_USER;

    if (!VirtualMemoryMapPage(testVirtualAddress, secondPhysicalAddress, userFlags))
    {
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Virtual address could not be reused after unmapping.\n");
        return 0;
    }

    if (!VirtualMemoryGetPageFlags(testVirtualAddress, &mappedFlags))
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Could not query reused page permissions.\n");
        return 0;
    }

    if ((mappedFlags & (VIRTUAL_MEMORY_FLAG_WRITABLE | VIRTUAL_MEMORY_FLAG_USER)) != (VIRTUAL_MEMORY_FLAG_WRITABLE | VIRTUAL_MEMORY_FLAG_USER))
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Writable/user page permissions were not reported correctly.\n");
        return 0;
    }

    if (g_virtualMemoryNoExecuteSupported && (mappedFlags & VIRTUAL_MEMORY_FLAG_NO_EXECUTE) == 0)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: NX permission was lost when reusing the virtual address.\n");
        return 0;
    }

    if (!VirtualMemoryTranslate(testVirtualAddress, &translatedAddress) || translatedAddress != secondPhysicalAddress)
    {
        VirtualMemoryUnmapPage(testVirtualAddress);
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Reused virtual address translated incorrectly.\n");
        return 0;
    }

    if (!VirtualMemoryUnmapPage(testVirtualAddress))
    {
        PhysicalPageFree(secondPhysicalAddress);
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Could not unmap reused virtual address.\n");
        return 0;
    }

    if (!PhysicalPageFree(secondPhysicalAddress))
    {
        PhysicalPageFree(testPhysicalAddress);
        serial_write_string("ERROR: Could not free second virtual-memory test page.\n");
        return 0;
    }

    if (!PhysicalPageFree(testPhysicalAddress))
    {
        serial_write_string("ERROR: Could not free virtual-memory test page.\n");
        return 0;
    }

    serial_write_string("JollyOS: Virtual memory mapping, permissions and cleanup self-tests passed.\n");

    return 1;
}