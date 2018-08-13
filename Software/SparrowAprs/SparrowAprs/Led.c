#include <stm32f4xx_hal.h>
#include "Led.h"

void LedInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	// Enable clocks
	__GPIOB_CLK_ENABLE();
	
	// LED IO settings
	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_PULLUP;
	
	// LED 1
	GPIO_InitStructure.Pin = GPIO_PIN_3;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	// LED 2
	GPIO_InitStructure.Pin = GPIO_PIN_4;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

	// LED 3
	GPIO_InitStructure.Pin = GPIO_PIN_5;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	// Turn both Leds off for now
	LedOff(LED_1);
	LedOff(LED_2);
	LedOff(LED_3);
}

void LedToggle(const enum LED led)
{
	switch (led)
	{
		case LED_1:
			HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_3);
			break;
		
		case LED_2:
			HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_4);
			break;

		case LED_3:
			HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
			break;
	}
}

void LedOff(const enum LED led)
{
	switch (led)
	{
		case LED_1:
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
			break;
		
		case LED_2:
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
			break;

		case LED_3:
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
			break;
	}
}

void LedOn(const enum LED led)
{
	switch (led)
	{
		case LED_1:
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
			break;
		
		case LED_2:
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
			break;

		case LED_3:
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
			break;
	}
}