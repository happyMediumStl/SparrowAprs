#include <stm32f4xx_hal.h>

static TIM_HandleTypeDef syncroTimerHandle;

static volatile uint8_t syncroTimerValid = 0;

void GpsPpsInit(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	// Enable clocks
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_TIM3_CLK_ENABLE();
	
	// LED IO settings
	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_PULLDOWN;
	GPIO_InitStructure.Alternate = GPIO_AF2_TIM3;
	
	// PB0 PPS
	GPIO_InitStructure.Pin = GPIO_PIN_0;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

	// Configure timer
	syncroTimerHandle.Instance				= TIM3;
	syncroTimerHandle.Init.Period			= 0xffffffff;
	syncroTimerHandle.Init.Prescaler		= 84 - 1;
	syncroTimerHandle.Init.ClockDivision	= TIM_CLOCKDIVISION_DIV1;
	syncroTimerHandle.Init.CounterMode		= TIM_COUNTERMODE_UP;
	HAL_TIM_Base_Init(&syncroTimerHandle);
	
	// Reset the timer before we start
	//SyncroTimerReset();
	
	// Enable interrupts. We want to know when the timer rolls over
	HAL_NVIC_SetPriority(TIM3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(TIM3_IRQn);
	
	// We want update events
	__HAL_TIM_ENABLE_IT(&syncroTimerHandle, TIM_IT_UPDATE);

	// Start the timer
	HAL_TIM_Base_Start_IT(&syncroTimerHandle);
}

void TIM3_IRQHandler()
{
	// Check if this is an update flag
	if(__HAL_TIM_GET_FLAG(&syncroTimerHandle, TIM_FLAG_UPDATE))
	{
		// Clear the interrupt
		__HAL_TIM_CLEAR_FLAG(&syncroTimerHandle, TIM_FLAG_UPDATE);

		syncroTimerValid = 0;
	}
}

// Check if the timer is valid
uint8_t SyncroTimerIsValid(void)
{
	return syncroTimerValid;
}

uint32_t SyncroTimerGetTime(void)
{
	return TIM3->CNT;
}

// Reset the syncrotimer to render it valid again
void SyncroTimerReset(void)
{
	// Clear the timer
	TIM3->CNT = 0;
	
	// Time is now valid
	syncroTimerValid = 1;
}