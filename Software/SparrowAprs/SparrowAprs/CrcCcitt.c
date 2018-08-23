#include <stdint.h>

uint16_t CrcCcitt(const uint8_t* data, const uint32_t length)
{
	uint16_t crc = 0xffff;
	uint8_t temp;
	uint32_t i;

	for (i = 0; i < length; i++)
	{
		temp = *(data++);
		temp ^= crc & 0xff;
		temp ^= temp << 4;
		crc = ((((uint16_t)temp << 8) | (crc >> 8)) ^ (uint8_t)(temp >> 4) ^ ((uint16_t)temp << 3));
	}
	
	return ~crc;
}