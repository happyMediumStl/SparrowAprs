#include "Config.h"
#include "FlashConfig.h"

// Config defaults
const static ConfigT __ConfigDefaults = 
{
	// System settings
	{
		100,
		100,
		144.390f
	},

	// APRS settings
	{
		"KD0POQ",		// Callsign
		"WIDE2 1",		// Path
		11,				// SSID
		'/',			// Symbol table
		'O',			// Symbol
		45000,			// Slowest beacon period
		0,				// Use smart beacon
		25000,			// Most minimum beacon time if Smart Beacon is enabled
		65.0f,			// Smart Beacon weight speed
		165.0f,			// Smart beacon weight dSpeed
		65.0f			// Smart beacon weight dHeading
	}
};

void ConfigLoadDefaults(void)
{
	FlashConfigLoadFromMemory(&__ConfigDefaults);
}