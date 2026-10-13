#ifndef JOLLYOS_PHYSICAL_MEMORY_H
#define JOLLYOS_PHYSICAL_MEMORY_H

#include <stdint.h>
#include "JollyOsBootInfo.h"

typedef struct
{
    uint64_t ManagedPages;
    uint64_t FreePages;
    uint64_t ReservedPages;
    uint64_t AllocatedPages;
} JOLLYOS_PHYSICAL_MEMORY_STATS;

/* Initialises the physical page allocator using the post-ExitBootServices UEFI memory map. */
int PhysicalMemoryInitialize(const JOLLYOS_BOOT_INFO* bootInfo);

/* Returns the physical base address of one allocated 4096-byte page, or zero on failure. */
uint64_t PhysicalPageAllocate(void);

/* Frees a page previously returned by PhysicalPageAllocate; rejects reserved and duplicate frees. */
int PhysicalPageFree(uint64_t physicalAddress);

/* Reads current page counts. Returns zero until the allocator is initialised. */
int PhysicalMemoryGetStats(JOLLYOS_PHYSICAL_MEMORY_STATS* stats);

#endif
