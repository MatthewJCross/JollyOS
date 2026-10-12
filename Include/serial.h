
#ifndef JOLLYOS_SERIAL_H
#define JOLLYOS_SERIAL_H

void serial_initialize(void);
void serial_write_char(char character);
void serial_write_string(const char* text);

#endif