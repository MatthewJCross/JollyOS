
#include <stddef.h>
#include <stdint.h>

#include "KernelHeap.h"
#include "PhysicalMemory.h"
#include "VirtualMemory.h"
#include "serial.h"

#define KERNEL_HEAP_PAGE_SIZE 4096ULL
#define KERNEL_HEAP_ALIGNMENT 16ULL
#define KERNEL_HEAP_BASE_ADDRESS 0x0000500000000000ULL
#define KERNEL_HEAP_MAX_PAGES 256ULL
#define KERNEL_HEAP_BLOCK_SIGNATURE 0x4A48424CUL

typedef struct KernelHeapBlock
{
    size_t Size;
    uint32_t IsFree;
    uint32_t Signature;
    struct KernelHeapBlock* Next;
    struct KernelHeapBlock* Previous;
} KernelHeapBlock;

static KernelHeapBlock* g_kernelHeapFirstBlock = NULL;
static uint64_t g_kernelHeapPageCount = 0;
static int g_kernelHeapInitialized = 0;
static uint64_t g_kernelHeapTotalAllocations = 0;
static uint64_t g_kernelHeapFailedAllocations = 0;

static void KernelHeapWriteUnsigned(uint64_t value)
{
    char buffer[21];
    size_t index = sizeof(buffer);

    buffer[--index] = '\0';

    do
    {
        buffer[--index] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (value != 0);

    serial_write_string(&buffer[index]);
}

static size_t KernelHeapAlignSize(size_t size)
{
    return (size + (KERNEL_HEAP_ALIGNMENT - 1ULL)) & ~(size_t)(KERNEL_HEAP_ALIGNMENT - 1ULL);
}

static int KernelHeapAddPage(void)
{
    uint64_t physicalAddress;
    uint64_t virtualAddress;
    uint64_t flags;
    KernelHeapBlock* block;
    KernelHeapBlock* lastBlock;

    if (g_kernelHeapPageCount >= KERNEL_HEAP_MAX_PAGES)
    {
        return 0;
    }

    physicalAddress = PhysicalPageAllocate();

    if (physicalAddress == 0)
    {
        return 0;
    }

    virtualAddress = KERNEL_HEAP_BASE_ADDRESS + (g_kernelHeapPageCount * KERNEL_HEAP_PAGE_SIZE);

    flags = VIRTUAL_MEMORY_FLAG_WRITABLE;

    if (VirtualMemorySupportsNoExecute())
    {
        flags |= VIRTUAL_MEMORY_FLAG_NO_EXECUTE;
    }

    if (!VirtualMemoryMapPage(virtualAddress, physicalAddress, flags))
    {
        PhysicalPageFree(physicalAddress);
        return 0;
    }

    block = (KernelHeapBlock*)(uintptr_t)virtualAddress;
    block->Size = (size_t)(KERNEL_HEAP_PAGE_SIZE - sizeof(KernelHeapBlock));
    block->IsFree = 1;
    block->Signature = KERNEL_HEAP_BLOCK_SIGNATURE;
    block->Next = NULL;
    block->Previous = NULL;

    if (g_kernelHeapFirstBlock == NULL)
    {
        g_kernelHeapFirstBlock = block;
    }
    else
    {
        lastBlock = g_kernelHeapFirstBlock;

        while (lastBlock->Next != NULL)
        {
            lastBlock = lastBlock->Next;
        }

        lastBlock->Next = block;
        block->Previous = lastBlock;

        if (lastBlock->IsFree)
        {
            uint8_t* lastBlockEnd = (uint8_t*)lastBlock + sizeof(KernelHeapBlock) + lastBlock->Size;

            if (lastBlockEnd == (uint8_t*)block)
            {
                lastBlock->Size += sizeof(KernelHeapBlock) + block->Size;
                lastBlock->Next = NULL;
            }
        }
    }

    g_kernelHeapPageCount++;

    return 1;
}

int KernelHeapInitialize(void)
{
    if (g_kernelHeapInitialized)
    {
        return 1;
    }

    if (!VirtualMemoryInitialize())
    {
        serial_write_string("ERROR: Cannot initialise heap before virtual memory.\n");
        return 0;
    }

    if (!KernelHeapAddPage())
    {
        serial_write_string("ERROR: Could not allocate initial kernel heap page.\n");
        return 0;
    }

    g_kernelHeapInitialized = 1;

    serial_write_string("JollyOS: Kernel heap initialised.\n");

    return 1;
}

int KernelHeapValidate(void)
{
    KernelHeapBlock* block;
    KernelHeapBlock* previousBlock;
    uintptr_t heapStart;
    uintptr_t heapEnd;
    uintptr_t blockAddress;
    uintptr_t blockEnd;
    uint64_t blockCount = 0;

    if (!g_kernelHeapInitialized || g_kernelHeapPageCount == 0)
    {
        return 0;
    }

    heapStart = (uintptr_t)KERNEL_HEAP_BASE_ADDRESS;
    heapEnd = heapStart + (uintptr_t)(g_kernelHeapPageCount * KERNEL_HEAP_PAGE_SIZE);

    if (heapEnd < heapStart)
    {
        return 0;
    }

    block = g_kernelHeapFirstBlock;
    previousBlock = NULL;

    while (block != NULL)
    {
        blockAddress = (uintptr_t)block;

        if (blockAddress < heapStart || blockAddress > heapEnd - sizeof(KernelHeapBlock))
        {
            return 0;
        }

        if ((blockAddress & (KERNEL_HEAP_ALIGNMENT - 1ULL)) != 0)
        {
            return 0;
        }

        if (block->Previous != previousBlock)
        {
            return 0;
        }

        if (block->IsFree != 0 && block->IsFree != 1)
        {
            return 0;
        }

        if (block->Signature != KERNEL_HEAP_BLOCK_SIGNATURE)
        {
            return 0;
        }

        if ((block->Size & (KERNEL_HEAP_ALIGNMENT - 1ULL)) != 0)
        {
            return 0;
        }

        if (block->Size > heapEnd - blockAddress - sizeof(KernelHeapBlock))
        {
            return 0;
        }

        blockEnd = blockAddress + sizeof(KernelHeapBlock) + block->Size;

        if (block->Next != NULL)
        {
            if ((uintptr_t)block->Next != blockEnd)
            {
                return 0;
            }
        }
        else if (blockEnd != heapEnd)
        {
            return 0;
        }

        previousBlock = block;
        block = block->Next;
        blockCount++;

        if (blockCount > (g_kernelHeapPageCount * KERNEL_HEAP_PAGE_SIZE) / sizeof(KernelHeapBlock))
        {
            return 0;
        }
    }

    return g_kernelHeapFirstBlock != NULL;
}

int KernelHeapGetStatistics(KernelHeapStatistics* statistics)
{
    KernelHeapBlock* block;

    if (statistics == NULL || !KernelHeapValidate())
    {
        return 0;
    }

    statistics->TotalPages = g_kernelHeapPageCount;
    statistics->AllocatedBlocks = 0;
    statistics->FreeBlocks = 0;
    statistics->AllocatedBytes = 0;
    statistics->FreeBytes = 0;
    statistics->TotalAllocations = g_kernelHeapTotalAllocations;
    statistics->FailedAllocations = g_kernelHeapFailedAllocations;

    block = g_kernelHeapFirstBlock;

    while (block != NULL)
    {
        if (block->IsFree)
        {
            statistics->FreeBlocks++;
            statistics->FreeBytes += block->Size;
        }
        else
        {
            statistics->AllocatedBlocks++;
            statistics->AllocatedBytes += block->Size;
        }

        block = block->Next;
    }

    return 1;
}

void* KernelHeapAllocate(size_t size)
{
    KernelHeapBlock* block;
    KernelHeapBlock* splitBlock;
    size_t alignedSize;

    if (!g_kernelHeapInitialized || size == 0)
    {
        g_kernelHeapFailedAllocations++;
        return NULL;
    }

    if (!KernelHeapValidate())
    {
        g_kernelHeapFailedAllocations++;
        return NULL;
    }

    if (size > (size_t)-1 - (KERNEL_HEAP_ALIGNMENT - 1ULL))
    {
        g_kernelHeapFailedAllocations++;
        return NULL;
    }

    alignedSize = KernelHeapAlignSize(size);

    for (;;)
    {
        block = g_kernelHeapFirstBlock;

        while (block != NULL)
        {
            if (block->IsFree && block->Size >= alignedSize)
            {
                if (block->Size >= alignedSize + sizeof(KernelHeapBlock) + KERNEL_HEAP_ALIGNMENT)
                {
                    splitBlock = (KernelHeapBlock*)((uint8_t*)block + sizeof(KernelHeapBlock) + alignedSize);

                    splitBlock->Size = block->Size - alignedSize - sizeof(KernelHeapBlock);
                    splitBlock->IsFree = 1;
                    splitBlock->Signature = KERNEL_HEAP_BLOCK_SIGNATURE;
                    splitBlock->Next = block->Next;
                    splitBlock->Previous = block;

                    if (splitBlock->Next != NULL)
                    {
                        splitBlock->Next->Previous = splitBlock;
                    }

                    block->Next = splitBlock;
                    block->Size = alignedSize;
                }

                block->IsFree = 0;
                g_kernelHeapTotalAllocations++;

                return (uint8_t*)block + sizeof(KernelHeapBlock);
            }

            block = block->Next;
        }

        if (!KernelHeapAddPage())
        {
            g_kernelHeapFailedAllocations++;
            return NULL;
        }
    }
}

int KernelHeapFree(void* pointer)
{
    KernelHeapBlock* block;
    KernelHeapBlock* nextBlock;
    KernelHeapBlock* previousBlock;
    void* expectedPointer;

    if (!g_kernelHeapInitialized || pointer == NULL)
    {
        return 0;
    }

    if (!KernelHeapValidate())
    {
        return 0;
    }

    block = g_kernelHeapFirstBlock;

    while (block != NULL)
    {
        expectedPointer = (uint8_t*)block + sizeof(KernelHeapBlock);

        if (expectedPointer == pointer)
        {
            break;
        }

        block = block->Next;
    }

    if (block == NULL || block->IsFree)
    {
        return 0;
    }

    block->IsFree = 1;

    nextBlock = block->Next;

    if (nextBlock != NULL && nextBlock->IsFree)
    {
        uint8_t* blockEnd = (uint8_t*)block + sizeof(KernelHeapBlock) + block->Size;

        if (blockEnd == (uint8_t*)nextBlock)
        {
            block->Size += sizeof(KernelHeapBlock) + nextBlock->Size;
            block->Next = nextBlock->Next;

            if (block->Next != NULL)
            {
                block->Next->Previous = block;
            }
        }
    }

    previousBlock = block->Previous;

    if (previousBlock != NULL && previousBlock->IsFree)
    {
        uint8_t* previousBlockEnd = (uint8_t*)previousBlock + sizeof(KernelHeapBlock) + previousBlock->Size;

        if (previousBlockEnd == (uint8_t*)block)
        {
            previousBlock->Size += sizeof(KernelHeapBlock) + block->Size;
            previousBlock->Next = block->Next;

            if (previousBlock->Next != NULL)
            {
                previousBlock->Next->Previous = previousBlock;
            }
        }
    }

    return 1;
}

int KernelHeapSelfTest(void)
{
    uint8_t* firstAllocation;
    uint8_t* secondAllocation;
    uint8_t* reusedAllocation;
    KernelHeapStatistics statistics;
    uint8_t* blockA;
    uint8_t* blockB;
    uint8_t* blockC;
    uint8_t* largeAllocation;

    if (!KernelHeapValidate())
    {
        serial_write_string("ERROR: Kernel heap integrity check failed at test start.\n");
        return 0;
    }

    if (KernelHeapAllocate(0) != NULL)
    {
        serial_write_string("ERROR: Kernel heap accepted a zero-size allocation.\n");
        return 0;
    }

    firstAllocation = (uint8_t*)KernelHeapAllocate(31);

    if (firstAllocation == NULL)
    {
        serial_write_string("ERROR: Kernel heap first allocation failed.\n");
        return 0;
    }

    secondAllocation = (uint8_t*)KernelHeapAllocate(96);

    if (secondAllocation == NULL)
    {
        KernelHeapFree(firstAllocation);
        serial_write_string("ERROR: Kernel heap second allocation failed.\n");
        return 0;
    }

    if (((uintptr_t)firstAllocation & (KERNEL_HEAP_ALIGNMENT - 1ULL)) != 0 || ((uintptr_t)secondAllocation & (KERNEL_HEAP_ALIGNMENT - 1ULL)) != 0)
    {
        KernelHeapFree(secondAllocation);
        KernelHeapFree(firstAllocation);
        serial_write_string("ERROR: Kernel heap alignment test failed.\n");
        return 0;
    }

    for (size_t index = 0; index < 31; index++)
    {
        firstAllocation[index] = (uint8_t)(index ^ 0x5A);
    }

    for (size_t index = 0; index < 96; index++)
    {
        secondAllocation[index] = (uint8_t)(index ^ 0xA5);
    }

    for (size_t index = 0; index < 31; index++)
    {
        if (firstAllocation[index] != (uint8_t)(index ^ 0x5A))
        {
            KernelHeapFree(secondAllocation);
            KernelHeapFree(firstAllocation);
            serial_write_string("ERROR: Kernel heap first allocation data test failed.\n");
            return 0;
        }
    }

    for (size_t index = 0; index < 96; index++)
    {
        if (secondAllocation[index] != (uint8_t)(index ^ 0xA5))
        {
            KernelHeapFree(secondAllocation);
            KernelHeapFree(firstAllocation);
            serial_write_string("ERROR: Kernel heap second allocation data test failed.\n");
            return 0;
        }
    }

    if (!KernelHeapFree(firstAllocation))
    {
        KernelHeapFree(secondAllocation);
        serial_write_string("ERROR: Kernel heap first free failed.\n");
        return 0;
    }

    if (!KernelHeapFree(secondAllocation))
    {
        serial_write_string("ERROR: Kernel heap second free failed.\n");
        return 0;
    }

    if (KernelHeapFree(secondAllocation))
    {
        serial_write_string("ERROR: Kernel heap accepted a double free.\n");
        return 0;
    }

    reusedAllocation = (uint8_t*)KernelHeapAllocate(64);

    if (reusedAllocation == NULL)
    {
        serial_write_string("ERROR: Kernel heap reuse allocation failed.\n");
        return 0;
    }

    if (!KernelHeapFree(reusedAllocation))
    {
        serial_write_string("ERROR: Kernel heap reuse free failed.\n");
        return 0;
    }

    blockA = (uint8_t*)KernelHeapAllocate(64);
    blockB = (uint8_t*)KernelHeapAllocate(128);
    blockC = (uint8_t*)KernelHeapAllocate(256);

    if (blockA == NULL || blockB == NULL || blockC == NULL)
    {
        if (blockA != NULL)
        {
            KernelHeapFree(blockA);
        }

        if (blockB != NULL)
        {
            KernelHeapFree(blockB);
        }

        if (blockC != NULL)
        {
            KernelHeapFree(blockC);
        }

        serial_write_string("ERROR: Kernel heap block splitting test allocation failed.\n");
        return 0;
    }

    blockA[0] = 0x11;
    blockB[0] = 0x22;
    blockC[0] = 0x33;

    if (!KernelHeapFree(blockB))
    {
        serial_write_string("ERROR: Kernel heap middle block free failed.\n");
        return 0;
    }

    if (blockA[0] != 0x11 || blockC[0] != 0x33)
    {
        serial_write_string("ERROR: Kernel heap neighbouring allocations were corrupted.\n");
        return 0;
    }

    if (!KernelHeapFree(blockA))
    {
        serial_write_string("ERROR: Kernel heap first block coalescing failed.\n");
        return 0;
    }

    if (!KernelHeapFree(blockC))
    {
        serial_write_string("ERROR: Kernel heap final block coalescing failed.\n");
        return 0;
    }

    if (!KernelHeapValidate())
    {
        serial_write_string("ERROR: Kernel heap integrity failed after coalescing.\n");
        return 0;
    }

    serial_write_string("JollyOS: Kernel heap block splitting and coalescing tests passed.\n");


    largeAllocation = (uint8_t*)KernelHeapAllocate(6000);

    if (largeAllocation == NULL)
    {
        serial_write_string("ERROR: Kernel heap multi-page allocation failed.\n");
        return 0;
    }

    largeAllocation[0] = 0x12;
    largeAllocation[2048] = 0x34;
    largeAllocation[4095] = 0x56;
    largeAllocation[5999] = 0x78;

    if (largeAllocation[0] != 0x12 || largeAllocation[2048] != 0x34 || largeAllocation[4095] != 0x56 || largeAllocation[5999] != 0x78)
    {
        KernelHeapFree(largeAllocation);
        serial_write_string("ERROR: Kernel heap multi-page data test failed.\n");
        return 0;
    }

    if (!KernelHeapFree(largeAllocation))
    {
        serial_write_string("ERROR: Kernel heap multi-page free failed.\n");
        return 0;
    }

    if (!KernelHeapValidate())
    {
        serial_write_string("ERROR: Kernel heap integrity failed after multi-page allocation.\n");
        return 0;
    }

    serial_write_string("JollyOS: Kernel heap multi-page allocation test passed.\n");

    if (!KernelHeapValidate())
    {
        serial_write_string("ERROR: Kernel heap integrity check failed after tests.\n");
        return 0;
    }

    if (!KernelHeapGetStatistics(&statistics))
    {
        serial_write_string("ERROR: Kernel heap statistics validation failed.\n");
        return 0;
    }

    if (statistics.AllocatedBlocks != 0)
    {
        serial_write_string("ERROR: Kernel heap has unexpected outstanding allocations.\n");
        return 0;
    }

    if (statistics.TotalPages < 2)
    {
        serial_write_string("ERROR: Kernel heap did not grow for the large allocation.\n");
        return 0;
    }

    if (statistics.FreeBlocks != 1)
    {
        serial_write_string("ERROR: Kernel heap free blocks were not fully coalesced.\n");
        return 0;
    }

    if (statistics.TotalAllocations < 3)
    {
        serial_write_string("ERROR: Kernel heap allocation counter test failed.\n");
        return 0;
    }

    if (statistics.FailedAllocations == 0)
    {
        serial_write_string("ERROR: Kernel heap failure counter test failed.\n");
        return 0;
    }

    serial_write_string("JollyOS: Kernel heap statistics\n");
    serial_write_string("  Heap pages: ");
    KernelHeapWriteUnsigned(statistics.TotalPages);
    serial_write_string("\n");
    serial_write_string("  Allocated blocks: ");
    KernelHeapWriteUnsigned(statistics.AllocatedBlocks);
    serial_write_string("\n");
    serial_write_string("  Free blocks: ");
    KernelHeapWriteUnsigned(statistics.FreeBlocks);
    serial_write_string("\n");
    serial_write_string("  Allocated bytes: ");
    KernelHeapWriteUnsigned(statistics.AllocatedBytes);
    serial_write_string("\n");
    serial_write_string("  Free bytes: ");
    KernelHeapWriteUnsigned(statistics.FreeBytes);
    serial_write_string("\n");
    serial_write_string("  Total allocations: ");
    KernelHeapWriteUnsigned(statistics.TotalAllocations);
    serial_write_string("\n");
    serial_write_string("  Failed allocations: ");
    KernelHeapWriteUnsigned(statistics.FailedAllocations);
    serial_write_string("\n");

    serial_write_string("JollyOS: Kernel heap allocation, alignment, reuse and free tests passed.\n");

    return 1;
}