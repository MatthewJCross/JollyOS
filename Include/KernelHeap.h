#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct KernelHeapStatistics
{
    uint64_t TotalPages;
    uint64_t AllocatedBlocks;
    uint64_t FreeBlocks;
    uint64_t AllocatedBytes;
    uint64_t FreeBytes;
    uint64_t TotalAllocations;
    uint64_t FailedAllocations;
} KernelHeapStatistics;

int KernelHeapInitialize(void);
void* KernelHeapAllocate(size_t size);
int KernelHeapFree(void* pointer);

int KernelHeapValidate(void);
int KernelHeapGetStatistics(KernelHeapStatistics* statistics);

int KernelHeapSelfTest(void);