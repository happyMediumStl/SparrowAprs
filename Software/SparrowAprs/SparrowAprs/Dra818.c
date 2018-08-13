#include <stm32f4xx_hal.h>
#include <stdio.h>

static UART_HandleTypeDef UartHandle;

static uint8_t xBuffer[200];

// PA2/PA3
void Dra818Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	// Enable clocks
	__GPIOA_CLK_ENABLE();
	__USART2_CLK_ENABLE();
	
	// Configure GPIO
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART2;

	// TX
	GPIO_InitStruct.Pin       = GPIO_PIN_2;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// RX
	GPIO_InitStruct.Pin       = GPIO_PIN_3;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	// Configure the USART peripheral
	UartHandle.Instance          = USART2;
	UartHandle.Init.BaudRate     = 9600;
	UartHandle.Init.WordLength   = UART_WORDLENGTH_8B;
	UartHandle.Init.StopBits     = UART_STOPBITS_1;
	UartHandle.Init.Parity       = UART_PARITY_NONE;
	UartHandle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
	UartHandle.Init.Mode         = UART_MODE_TX_RX;

	// Commit the USART
	if(HAL_UART_Init(&UartHandle) != HAL_OK)
	{
		while (1) ;
	}
}

uint8_t Dra818SetGroup(const uint8_t bw, const float txFreq, const float rxFreq, const char ctcssTx[4], const char ctcssRx[4], const uint8_t squelch)
{
	uint32_t len;

	// Pack the buffer
	len = sprintf((char*)&xBuffer, "AT+DMOSETGROUP=%c,%f,%f,%.4s,%c,%.4s\r\n", bw + '0', txFreq, rxFreq, ctcssTx, squelch + '0', ctcssRx);

	// Send
	HAL_UART_Transmit(&UartHandle, xBuffer, len - 1, 5000);

	return 0;
}