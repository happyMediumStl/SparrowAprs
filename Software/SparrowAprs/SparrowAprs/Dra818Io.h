#ifndef DRA818IO_H
#define DRA818IO_H
#include <stdint.h>

void Dra818IoInit(void);
void Dra818IoPttOn(void);
void Dra818IoPttOff(void);
void Dra818IoPowerDown(void);
void Dra818IoPowerUp(void);
void Dra818IoSetHighRfPower(void);
void Dra818IoSetLowRfPower(void);
uint8_t Dra818IoIsSq(void);

#endif // !DRA818IO_H
