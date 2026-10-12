#include <stdint.h>
#include <stddef.h>
#include <intrin.h>

#include "JollyOsBootInfo.h"
#include "serial.h"

static volatile uint8_t *g_framebuffer;
static uint32_t g_width;
static uint32_t g_height;
static uint32_t g_pixelsPerScanLine;
static uint32_t g_pixelFormat;

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
	                FillRectangle(
	                    x + column * scale,
	                    y + row * scale,
	                    scale,
	                    scale,
	                    red,
	                    green,
	                    blue);
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