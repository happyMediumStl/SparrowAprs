#include <stm32f4xx_hal.h>
#include <stdio.h>
#include <sys/stat.h>

UART_HandleTypeDef UartHandle;

static void putC(const char c);

// PB6
void RetargetInit(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	// Enable clocks
	__GPIOB_CLK_ENABLE();
	__USART1_CLK_ENABLE();
	
	// Configure GPIO
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Pin       = GPIO_PIN_6;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	// Configure the USART peripheral
	UartHandle.Instance          = USART1;
	UartHandle.Init.BaudRate     = 115200;
	UartHandle.Init.WordLength   = UART_WORDLENGTH_8B;
	UartHandle.Init.StopBits     = UART_STOPBITS_1;
	UartHandle.Init.Parity       = UART_PARITY_NONE;
	UartHandle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
	UartHandle.Init.Mode         = UART_MODE_TX;

	// Commit the USART
	if(HAL_UART_Init(&UartHandle) != HAL_OK)
	{
		while (1) ;
	}

	// Do not buffer stdout
	setvbuf(stdout, NULL, _IONBF, 0);
}

static void putC(const char c)
{
	// Wait while there is unsent data in the TX buffer
	while(!(__HAL_UART_GET_FLAG(&UartHandle, UART_FLAG_TXE) != RESET));
	
	// Write the new char to the TX buffer
	UartHandle.Instance->DR = (uint32_t)(c & 0x1FF);
}

int _fstat(int fd, struct stat *pStat)
{
	pStat->st_mode = S_IFCHR;
	return 0;
}
 
int _close(int a)
{
	return -1;
}
 
int _write(int fd, char *pBuffer, int size)
{
	for (int i = 0; i < size; i++)
	{
		putC(pBuffer[i]);
	}
	
	return size;
}
 
int _isatty(int fd)
{
	return 1;
}
 
int _lseek(int a, int b, int c)
{
	return -1;
}
 
int _read(int fd, char *pBuffer, int size)
{
	return size;
}