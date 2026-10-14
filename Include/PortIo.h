#pragma once

#include <stdint.h>

uint8_t IoRead8(uint16_t port);
void IoWrite8(uint16_t port, uint8_t value);