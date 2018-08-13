#ifndef RTC_H
#define RTC_H

#include <stdint.h>

void RtcInit(void);
void RtcSet(const uint32_t now);
uint32_t RtcGet(void);

#endif // !RTC_H
