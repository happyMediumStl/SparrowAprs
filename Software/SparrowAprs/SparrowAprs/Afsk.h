#ifndef AFSK_H
#include <stdint.h>

uint32_t AfskHdlcEncode(const uint8_t* data, const uint32_t len, const uint32_t startStuff, const uint32_t endStuff, uint8_t* afskOut, const uint32_t maxLen);

#endif // !AFSK_H
