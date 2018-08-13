#ifndef DRA818_H
#define DRA818_H

#include <stdint.h>

void Dra818Init(void);
uint8_t Dra818SetGroup(const uint8_t bw, const float txFreq, const float rxFreq, const char ctcssTx[4], const char ctcssRx[4], const uint8_t squelch);

#endif // !DRA818_H
