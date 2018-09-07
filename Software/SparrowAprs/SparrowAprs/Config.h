#ifndef CONFIG_H
#define CONFIG_H

#include <stm32f4xx_hal.h>
#include <stdint.h>

// Config struct
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
		uint8_t Path[7];
		uint8_t Ssid;
		uint8_t SymbolTable;
		uint8_t Symbol;
		uint32_t BeaconPeriod;
		uint8_t UseSmartBeacon;
		uint32_t SmartBeaconMinimumBeaconPeriod;
		float SmartBeaconHeadingTuning;
		float SmartBeaconSpeedTuning;
		float SmartBeaconWeightSpeed;
		float SmartBeaconWeightdSpeed;
		float SmartBeaconWeightdHeading;
	} Aprs;
} ConfigT;

// Set the location of the stored configuration structure in flash
#define FLASH_CONFIG_SECTOR FLASH_SECTOR_10

void ConfigLoadDefaults(void);

#endif // !CONFIG_H
