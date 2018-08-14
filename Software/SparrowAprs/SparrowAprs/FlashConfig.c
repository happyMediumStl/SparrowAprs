#include <stm32f4xx_hal.h>
#include <string.h>
#include "Config.h"
#include "FlashConfig.h"

static void* Stm32f4SectorToBaseAddr(const uint32_t sector);

typedef struct 
{
	uint8_t Sector;
	uint32_t* Address;
	uint32_t Size;
} FlashSectorT;

// STM32F4 FLASH Sectors table
#define SECTOR_COUNT	12
const FlashSectorT _Sectors[SECTOR_COUNT] = {
	{0,  (uint32_t*)0x08000000, 16000},
	{1,  (uint32_t*)0x08004000, 16000},
	{2,  (uint32_t*)0x08008000, 16000},
	{3,  (uint32_t*)0x0800C000, 16000},
	{4,  (uint32_t*)0x08010000, 64000},
	{5,  (uint32_t*)0x08020000, 128000},
	{6,  (uint32_t*)0x08040000, 128000},
	{7,  (uint32_t*)0x08060000, 128000},
	{8,  (uint32_t*)0x08080000, 128000},
	{9,  (uint32_t*)0x080a0000, 128000},
	{10, (uint32_t*)0x080c0000, 128000},
	{11, (uint32_t*)0x080e0000, 128000}
};

typedef struct
{
	uint32_t FlashMagic;
	ConfigT Config;
	uint32_t Crc;
} FlashConfigStructureT;

// Storage for the live configuration structure 
static FlashConfigStructureT config;

#define FLASH_CONFIG_MAGIC 0x51242757

void FlashConfigInit(void)
{
	// If we can't load from flash, then just load defaults
	if (!FlashConfigLoad())
	{
		ConfigLoadDefaults();
	}
}

uint8_t FlashConfigLoad(void)
{
	FlashConfigStructureT* storedStruct = Stm32f4SectorToBaseAddr(FLASH_CONFIG_SECTOR);

	if (storedStruct->FlashMagic != FLASH_CONFIG_MAGIC)
	{
		return 0;
	}

	memcpy(&config, &storedStruct->Config, sizeof(ConfigT));

	return 1;
}

void FlashConfigLoadFromMemory(const ConfigT* configIn)
{
	memcpy(&config.Config, configIn, sizeof(ConfigT));
}

// Save config to flash
uint8_t FlashConfigSave(void)
{
	uint32_t i = 0;
	uint32_t* addrPtr = Stm32f4SectorToBaseAddr(FLASH_CONFIG_SECTOR);

	// Unlock FLASH
	HAL_FLASH_Unlock();

	// Program the structure
	for(i = 0 ; i < sizeof(FlashConfigStructureT) ; i++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, *addrPtr++, *(uint64_t*)(&config) + i);
	}

	// Lock FLASH
	HAL_FLASH_Lock();

	return 1;
}

uint32_t GetLatestFreeConfigAddress(void)
{
	return 0;
}

// Get the latest flash config structure
uint32_t GetLatestConfigStructure(void)
{
	return 0;
}

// Get the base address of a sector
static void* Stm32f4SectorToBaseAddr(const uint32_t sector)
{
	if (sector >= SECTOR_COUNT)
	{
		return 0;
	}

	return _Sectors[sector].Address;
}

// Get pointer to the live configuration struct in memory
ConfigT* FlashConfigGetPtr(void)
{
	return &config.Config;
}
