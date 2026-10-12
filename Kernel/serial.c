#include <stdint.h>
#include <intrin.h>
#include "serial.h"

#define COM1_PORT 0x3F8

#define SERIAL_DATA(base) ((uint16_t)((base) + 0))
#define SERIAL_INTERRUPT_ENABLE(base) ((uint16_t)((base) + 1))
#define SERIAL_FIFO_CONTROL(base) ((uint16_t)((base) + 2))
#define SERIAL_LINE_CONTROL(base) ((uint16_t)((base) + 3))
#define SERIAL_MODEM_CONTROL(base) ((uint16_t)((base) + 4))
#define SERIAL_LINE_STATUS(base) ((uint16_t)((base) + 5))

static void port_out8(uint16_t port, uint8_t value)
{
    __outbyte(port, value);
}

static uint8_t port_in8(uint16_t port)
{
    return __inbyte(port);
}

void serial_initialize(void)
{
    port_out8(SERIAL_INTERRUPT_ENABLE(COM1_PORT), 0x00);
    port_out8(SERIAL_LINE_CONTROL(COM1_PORT), 0x80);
    port_out8(SERIAL_DATA(COM1_PORT), 0x01);
    port_out8(SERIAL_INTERRUPT_ENABLE(COM1_PORT), 0x00);
    port_out8(SERIAL_LINE_CONTROL(COM1_PORT), 0x03);
    port_out8(SERIAL_FIFO_CONTROL(COM1_PORT), 0xC7);
    port_out8(SERIAL_MODEM_CONTROL(COM1_PORT), 0x0B);
}

void serial_write_char(char character)
{
    uint32_t timeout = 1000000;

    while ((port_in8(SERIAL_LINE_STATUS(COM1_PORT)) & 0x20) == 0)
    {
        if (--timeout == 0)
        {
            return;
        }
    }

    port_out8(SERIAL_DATA(COM1_PORT), (uint8_t)character);
}

void serial_write_string(const char* text)
{
    if (text == 0)
    {
        return;
    }

    while (*text != '\0')
    {
        if (*text == '\n')
        {
            serial_write_char('\r');
        }

        serial_write_char(*text);
        text++;
    }
}