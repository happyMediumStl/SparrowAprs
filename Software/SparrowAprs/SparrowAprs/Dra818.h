#ifndef DRA818_H
#define DRA818_H

#include <stdint.h>

void Dra818Init(void);
uint8_t Dra818Connect(void);
uint8_t Dra818SetGroup(const float txFreq, const float rxFreq, const char ctcssTx[4], const char ctcssRx[4], const uint8_t squelch);
uint8_t Dra818SetFilter(const uint8_t predeemphasis, const uint8_t highpass, const uint8_t lowpass);

#endif // !DRA818_H
