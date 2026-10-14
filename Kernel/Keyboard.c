#include <stdint.h>

#include "PortIo.h"
#include "Keyboard.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

#define KEYBOARD_STATUS_OUTPUT_FULL 0x01
#define KEYBOARD_STATUS_AUXILIARY 0x20

static int g_leftShift;
static int g_rightShift;
static int g_extendedCode;
static int g_pauseBytesRemaining;
static int g_capsLock;
static int g_capsLockDown;

static int IsShiftPressed(void)
{
    return g_leftShift || g_rightShift;
}

static char TranslateScanCode(uint8_t scanCode, int shifted)
{
    switch (scanCode)
    {
    case 0x02: return shifted ? '!' : '1';
    case 0x03: return shifted ? '"' : '2';
    case 0x04: return shifted ? 0xA3 : '3';
    case 0x05: return shifted ? '$' : '4';
    case 0x06: return shifted ? '%' : '5';
    case 0x07: return shifted ? '^' : '6';
    case 0x08: return shifted ? '&' : '7';
    case 0x09: return shifted ? '*' : '8';
    case 0x0A: return shifted ? '(' : '9';
    case 0x0B: return shifted ? ')' : '0';
    case 0x0C: return shifted ? '_' : '-';
    case 0x0D: return shifted ? '+' : '=';
    case 0x0E: return '\b';
    case 0x0F: return '\t';

    case 0x10: return shifted ? 'Q' : 'q';
    case 0x11: return shifted ? 'W' : 'w';
    case 0x12: return shifted ? 'E' : 'e';
    case 0x13: return shifted ? 'R' : 'r';
    case 0x14: return shifted ? 'T' : 't';
    case 0x15: return shifted ? 'Y' : 'y';
    case 0x16: return shifted ? 'U' : 'u';
    case 0x17: return shifted ? 'I' : 'i';
    case 0x18: return shifted ? 'O' : 'o';
    case 0x19: return shifted ? 'P' : 'p';
    case 0x1A: return shifted ? '{' : '[';
    case 0x1B: return shifted ? '}' : ']';
    case 0x1C: return '\n';

    case 0x1E: return shifted ? 'A' : 'a';
    case 0x1F: return shifted ? 'S' : 's';
    case 0x20: return shifted ? 'D' : 'd';
    case 0x21: return shifted ? 'F' : 'f';
    case 0x22: return shifted ? 'G' : 'g';
    case 0x23: return shifted ? 'H' : 'h';
    case 0x24: return shifted ? 'J' : 'j';
    case 0x25: return shifted ? 'K' : 'k';
    case 0x26: return shifted ? 'L' : 'l';
    case 0x27: return shifted ? ':' : ';';
    case 0x28: return shifted ? '@' : '\'';
    case 0x29: return shifted ? 0xAC : '`';
    case 0x2B: return shifted ? '~' : '#';

    case 0x2C: return shifted ? 'Z' : 'z';
    case 0x2D: return shifted ? 'X' : 'x';
    case 0x2E: return shifted ? 'C' : 'c';
    case 0x2F: return shifted ? 'V' : 'v';
    case 0x30: return shifted ? 'B' : 'b';
    case 0x31: return shifted ? 'N' : 'n';
    case 0x32: return shifted ? 'M' : 'm';
    case 0x33: return shifted ? '<' : ',';
    case 0x34: return shifted ? '>' : '.';
    case 0x35: return shifted ? '?' : '/';
    case 0x39: return ' ';

    case 0x56: return shifted ? '|' : '\\';

    default:
        return 0;
    }
}

int KeyboardInitialize(void)
{
    g_leftShift = 0;
    g_rightShift = 0;
    g_extendedCode = 0;
    g_pauseBytesRemaining = 0;
    g_capsLock = 0;
    g_capsLockDown = 0;

    return 1;
}

int KeyboardPollCharacter(void)
{
    uint8_t status;
    uint8_t scanCode;
    int released;
    int shifted;
    char character;

    status = IoRead8(KEYBOARD_STATUS_PORT);

    if ((status & KEYBOARD_STATUS_OUTPUT_FULL) == 0)
    {
        return -1;
    }

    scanCode = IoRead8(KEYBOARD_DATA_PORT);

    if ((status & KEYBOARD_STATUS_AUXILIARY) != 0)
    {
        return -1;
    }

    if (g_pauseBytesRemaining != 0)
    {
        g_pauseBytesRemaining--;
        return -1;
    }

    if (scanCode == 0xE1)
    {
        g_pauseBytesRemaining = 5;
        g_extendedCode = 0;
        return -1;
    }

    if (scanCode == 0xE0)
    {
        g_extendedCode = 1;
        return -1;
    }

    if (g_extendedCode)
    {
        g_extendedCode = 0;
        return -1;
    }

    released = (scanCode & 0x80) != 0;
    scanCode &= 0x7F;

    if (scanCode == 0x2A)
    {
        g_leftShift = !released;
        return -1;
    }

    if (scanCode == 0x36)
    {
        g_rightShift = !released;
        return -1;
    }

    if (scanCode == 0x3A)
    {
        if (released)
        {
            g_capsLockDown = 0;
        }
        else if (!g_capsLockDown)
        {
            g_capsLock = !g_capsLock;
            g_capsLockDown = 1;
        }

        return -1;
    }

    if (released)
    {
        return -1;
    }

    shifted = IsShiftPressed();

    if ((scanCode >= 0x10 && scanCode <= 0x19) || (scanCode >= 0x1E && scanCode <= 0x26) || (scanCode >= 0x2C && scanCode <= 0x32))
    {
        shifted = shifted != g_capsLock;
    }

    character = TranslateScanCode(scanCode, shifted);

    if (character == 0)
    {
        return -1;
    }

    return (unsigned char)character;
}

