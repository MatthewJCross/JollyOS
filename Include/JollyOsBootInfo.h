
#ifndef JOLLYOS_BOOT_INFO_H
#define JOLLYOS_BOOT_INFO_H

#include <stdint.h>

#define JOLLYOS_BOOT_INFO_VERSION 1U

typedef struct
{
    uint32_t Version;
    uint32_t Size;

    uint64_t Framebuffer;
    uint32_t Width;
    uint32_t Height;
    uint32_t PixelsPerScanLine;
    uint32_t PixelFormat;

    uint64_t MemoryMap;
    uint64_t MemoryMapSize;
    uint64_t MemoryMapDescriptorSize;
    uint32_t MemoryMapDescriptorVersion;
    uint32_t Reserved;

    uint64_t KernelBase;
    uint64_t KernelSize;
} JOLLYOS_BOOT_INFO;

#endif