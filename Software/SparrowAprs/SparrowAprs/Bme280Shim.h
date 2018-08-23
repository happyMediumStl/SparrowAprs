#ifndef BME280SHIM_H

#include <stdint.h>

void Bme280ShimInit(void);
const uint8_t Bme280ShimGetTph(float* temperature, float* pressure, float* humidity);

#endif // !BME280SHIM_H
