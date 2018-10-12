#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>

uint8_t Hex2int(const uint8_t x);
float atofl(const uint8_t* s, const uint32_t length);
int32_t atoil(const uint8_t* buffer, const uint32_t length);
uint8_t Int2HexDigit(const uint8_t x);
uint8_t IsAscii(const uint8_t x);

#endif // !HELPERS_H
