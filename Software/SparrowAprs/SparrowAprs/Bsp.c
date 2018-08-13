#include <stm32f4xx_hal.h>

void BspInit(void)
{
	// Pa1 Vsense
	GPIO_InitTypeDef GPIO_InitStruct;

	// PA_4 DACOUT_1

	// Enable Clocks
	__GPIOA_CLK_ENABLE();
	__ADC1_CLK_ENABLE();

	// Configure DAC output pin
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

uint16_t BspGetVSense(void)
{
}