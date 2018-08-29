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
		"KD0POQ",
		"WIDE2 1",
		11,
		'/',
		'O',
		10000,
		0
	}
};

void ConfigLoadDefaults(void)
{
	FlashConfigLoadFromMemory(&__ConfigDefaults);
}