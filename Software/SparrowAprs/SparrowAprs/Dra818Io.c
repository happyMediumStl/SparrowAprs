#include <stm32f4xx_hal.h>
#include "Dra818Io.h"

void Dra818IoInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	// Enable clocks
	__GPIOA_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	
	// PTT IO settings
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_PULLDOWN;
	
	// PTT PA5
	GPIO_InitStructure.Pin = GPIO_PIN_5;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

	// PD :: Powerdown
	GPIO_InitStructure.Pin = GPIO_PIN_6;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

	// PS :: Power setting
	GPIO_InitStructure.Pin = GPIO_PIN_7;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

	// SQ in
	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Pin = GPIO_PIN_4;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

	// Set default states
	Dra818IoPttOff();
	Dra818IoPowerUp();
}

// Start TX
void Dra818IoPttOn(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

// Stop TX
void Dra818IoPttOff(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
}

// Powerdown
void Dra818IoPowerDown(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
}

// Powerup
void Dra818IoPowerUp(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
}

// RF Power Output Setting High
void Dra818IoSetHighRfPower(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
}

// RF Power Output Setting High
void Dra818IoSetLowRfPower(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
}

// SQ
uint8_t Dra818IoIsSq(void)
{
	return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4);
}
