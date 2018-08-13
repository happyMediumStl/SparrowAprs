#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct
{
	// System settings
	struct
	{
		uint32_t PrePttDelay;
		uint32_t PostPttDelay;
		float Frequency;
	} System;

	// APRS settings
	struct
	{
		uint8_t Callsign[6];
		uint8_t Ssid;
		uint8_t SymbolTable;
		uint8_t Symbol;
		
	} Aprs;
} ConfigT;

#endif // !CONFIG_H
