#ifndef HELPERS_H
#define HELPERS_H

#include <stdint.h>

uint8_t Hex2int(const uint8_t x);
float atofl(const uint8_t* s, const uint32_t length);
int32_t atoil(const uint8_t* buffer, const uint32_t length);
void printN(const uint8_t* x, const uint32_t y);

#endif // !HELPERS_H
