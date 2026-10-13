#pragma once

#include <stdint.h>

#define VIRTUAL_MEMORY_PAGE_SIZE 4096ULL

#define VIRTUAL_MEMORY_FLAG_WRITABLE       0x002ULL
#define VIRTUAL_MEMORY_FLAG_USER           0x004ULL
#define VIRTUAL_MEMORY_FLAG_WRITE_THROUGH  0x008ULL
#define VIRTUAL_MEMORY_FLAG_CACHE_DISABLE  0x010ULL
#define VIRTUAL_MEMORY_FLAG_GLOBAL         0x100ULL
#define VIRTUAL_MEMORY_FLAG_NO_EXECUTE     0x8000000000000000ULL

int VirtualMemoryInitialize(void);
int VirtualMemorySupportsNoExecute(void);

int VirtualMemoryTranslate(uint64_t virtualAddress, uint64_t* physicalAddress);
int VirtualMemoryGetPageFlags(uint64_t virtualAddress, uint64_t* flags);
int VirtualMemoryMapPage(uint64_t virtualAddress, uint64_t physicalAddress, uint64_t flags);
int VirtualMemoryUnmapPage(uint64_t virtualAddress);

int VirtualMemorySelfTest(void);