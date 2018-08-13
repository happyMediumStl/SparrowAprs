/*
	Audio input/output driver
		-- Chris Seto, 2018
*/

#include <stm32f4xx_hal.h>
#include "FreeRTOS.h"
#include "semphr.h"

// Peripheral handles
static DMA_HandleTypeDef dmaInHandle;
static DMA_HandleTypeDef dmaOutHandle;
static DAC_HandleTypeDef dacHandle;
static ADC_HandleTypeDef adcHandle;
static TIM_HandleTypeDef timerHandle;

#define AUDIO_SAMPLE_TICK_PERIOD	7000
static SemaphoreHandle_t audioOutSemiphore;
static SemaphoreHandle_t audioInSemiphore;

static void TimerInit(void)
{
	TIM_MasterConfigTypeDef sMasterConfig;

	// Enable clocks
	__TIM2_CLK_ENABLE();

	// Configure timer
	timerHandle.Instance			= TIM2;
	timerHandle.Init.Period			= AUDIO_SAMPLE_TICK_PERIOD - 1;
	timerHandle.Init.Prescaler		= 0;       
	timerHandle.Init.ClockDivision	= 0;    
	timerHandle.Init.CounterMode	= TIM_COUNTERMODE_UP; 
	HAL_TIM_Base_Init(&timerHandle);

	sMasterConfig.MasterOutputTrigger	= TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode		= TIM_MASTERSLAVEMODE_DISABLE;
	HAL_TIMEx_MasterConfigSynchronization(&timerHandle, &sMasterConfig);

	HAL_TIM_Base_Start(&timerHandle);
}

static void DacInit(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	static DAC_ChannelConfTypeDef sConfig;
	
	// PA_4 DACOUT_1
	// Enable Clocks
	__GPIOA_CLK_ENABLE();
	__DAC_CLK_ENABLE();
	__DMA1_CLK_ENABLE();
	
	// Configure DAC output pin
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Configure DAC
	dacHandle.Instance = DAC1;
	HAL_DAC_Init(&dacHandle);

	// Configure channel
	sConfig.DAC_Trigger = DAC_TRIGGER_T2_TRGO;
	sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	HAL_DAC_ConfigChannel(&dacHandle, &sConfig, DAC_CHANNEL_1);

	HAL_DAC_Start(&dacHandle, DAC_CHANNEL_1); 

	// Configure DMA
	// DMA1 channel 7 stream 6
	dmaOutHandle.Instance					= DMA1_Stream5;
	dmaOutHandle.Init.Channel				= DMA_CHANNEL_7;
	dmaOutHandle.Init.Direction			= DMA_MEMORY_TO_PERIPH;
	dmaOutHandle.Init.PeriphInc			= DMA_PINC_DISABLE;
	dmaOutHandle.Init.MemInc				= DMA_MINC_ENABLE;
	dmaOutHandle.Init.PeriphDataAlignment	= DMA_PDATAALIGN_BYTE;
	dmaOutHandle.Init.MemDataAlignment		= DMA_MDATAALIGN_BYTE;
	dmaOutHandle.Init.Mode					= DMA_NORMAL;
	dmaOutHandle.Init.Priority				= DMA_PRIORITY_HIGH;
	dmaOutHandle.Init.FIFOMode				= DMA_FIFOMODE_DISABLE;         
	dmaOutHandle.Init.FIFOThreshold		= DMA_FIFO_THRESHOLD_HALFFULL;
	dmaOutHandle.Init.MemBurst				= DMA_MBURST_SINGLE;
	dmaOutHandle.Init.PeriphBurst			= DMA_PBURST_SINGLE; 
	HAL_DMA_Init(&dmaOutHandle);

	__HAL_LINKDMA(&dacHandle, DMA_Handle1, dmaOutHandle);

	// Enable interrupts
	HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0xf, 0xf);
	HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

static void AdcInit(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	ADC_ChannelConfTypeDef sConfig;

	// Enable Clocks
	__GPIOC_CLK_ENABLE();
	__DMA2_CLK_ENABLE();
	__ADC3_CLK_ENABLE();

	// Configure DAC output pin
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;

	// ADC123_IN13
	GPIO_InitStruct.Pin = GPIO_PIN_3;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	adcHandle.Instance					 = ADC3;
	adcHandle.Init.ClockPrescaler        = ADC_CLOCKPRESCALER_PCLK_DIV4;
	adcHandle.Init.Resolution            = ADC_RESOLUTION_8B;
	adcHandle.Init.ScanConvMode          = DISABLE;
	adcHandle.Init.ContinuousConvMode    = ENABLE;
	adcHandle.Init.DiscontinuousConvMode = DISABLE;
	adcHandle.Init.NbrOfDiscConversion   = 0;
	adcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_RISING;
	adcHandle.Init.ExternalTrigConv      = ADC_EXTERNALTRIG2_T2_TRGO;
	adcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
	adcHandle.Init.NbrOfConversion       = 1;
	adcHandle.Init.DMAContinuousRequests = ENABLE;
	adcHandle.Init.EOCSelection          = DISABLE;
	HAL_ADC_Init(&adcHandle);

	// Configure in channel
	sConfig.Channel      = ADC_CHANNEL_13;
	sConfig.Rank         = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
	sConfig.Offset       = 0;
	HAL_ADC_ConfigChannel(&adcHandle, &sConfig);

	// DMA: DMA2, (Stream 0, Stream 1) channel 2
	// DMA1 channel 7 stream 6
	dmaInHandle.Instance					= DMA2_Stream0;
	dmaInHandle.Init.Channel				= DMA_CHANNEL_2;
	dmaInHandle.Init.Direction			= DMA_PERIPH_TO_MEMORY;
	dmaInHandle.Init.PeriphInc			= DMA_PINC_DISABLE;
	dmaInHandle.Init.MemInc				= DMA_MINC_ENABLE;
	dmaInHandle.Init.PeriphDataAlignment	= DMA_PDATAALIGN_BYTE;
	dmaInHandle.Init.MemDataAlignment		= DMA_MDATAALIGN_BYTE;
	dmaInHandle.Init.Mode					= DMA_NORMAL;
	dmaInHandle.Init.Priority				= DMA_PRIORITY_HIGH;
	dmaInHandle.Init.FIFOMode				= DMA_FIFOMODE_DISABLE;         
	dmaInHandle.Init.FIFOThreshold		= DMA_FIFO_THRESHOLD_HALFFULL;
	dmaInHandle.Init.MemBurst				= DMA_MBURST_SINGLE;
	dmaInHandle.Init.PeriphBurst			= DMA_PBURST_SINGLE; 
	HAL_DMA_Init(&dmaInHandle);

	__HAL_LINKDMA(&adcHandle, DMA_Handle, dmaInHandle);

	// Enable interrupts
	HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

// DAC 2
void AudioInit(void)
{
	// Init Timer
	TimerInit();

	// Init Audio Out
	DacInit();

	// Init Audio In
	//AdcInit();

	// Create the semiphore
	audioOutSemiphore = xSemaphoreCreateBinary();
	audioInSemiphore = xSemaphoreCreateBinary();
}

// Start recording
void AudioRecord(uint8_t* buffer, const uint32_t length)
{
	HAL_ADC_Start_DMA(&adcHandle, (uint32_t*)buffer, length);
}

// Stop recording and return the size of the buffer
uint32_t AudioInStopRecording(void)
{
	uint32_t size = DMA2_Stream0->NDTR;

	HAL_ADC_Stop_DMA(&adcHandle);

	return size;
}

void AudioPlay(const uint8_t* buffer, const uint32_t len)
{
	// Stop audio first
	HAL_DAC_Stop_DMA(&dacHandle, DAC_CHANNEL_1);

	// We already killed audio out, so just take the semiphore no matter what
	if (xSemaphoreTake(audioOutSemiphore, 0) == pdFALSE)
	{
		xSemaphoreGive(audioOutSemiphore);
	}

	// Start
	HAL_DAC_Start_DMA(&dacHandle, DAC_CHANNEL_1, (uint32_t*)buffer, len, DAC_ALIGN_8B_R);
}

uint8_t AudioIsPlaying(void)
{
	return (uxSemaphoreGetCount(audioOutSemiphore) == 0);
}

uint8_t AudioIsRecording(void)
{
	return (uxSemaphoreGetCount(audioOutSemiphore) == 0);
}

void AudioOutWait(const uint32_t ticks)
{
	xSemaphoreTake(audioOutSemiphore, ticks);
}

void AudioInWait(const uint32_t ticks)
{
	xSemaphoreTake(audioInSemiphore, ticks);
}

// Handle Audio In DMA
void DMA2_Stream0_IRQHandler(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	// Handle half transfer
	if(__HAL_DMA_GET_FLAG(&dmaInHandle, DMA_FLAG_TCIF2_6))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaInHandle, DMA_FLAG_TCIF2_6);
	}

	// Handle full transfer complete
	if(__HAL_DMA_GET_FLAG(&dmaInHandle, DMA_FLAG_HTIF2_6))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaInHandle, DMA_FLAG_HTIF2_6);
		xSemaphoreGiveFromISR(audioInSemiphore, &xHigherPriorityTaskWoken);
	}

	// Handle transfer error 
	if(__HAL_DMA_GET_FLAG(&dmaInHandle, DMA_FLAG_TEIF2_6))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaInHandle, DMA_FLAG_TEIF2_6);
	}

	// FIFO error (?)
	if(__HAL_DMA_GET_FLAG(&dmaInHandle, DMA_FLAG_FEIF2_6))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaInHandle, DMA_FLAG_FEIF2_6);
	}
}

// Handle Audio Out DMA
void DMA1_Stream5_IRQHandler(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	// Handle half transfer
	if(__HAL_DMA_GET_FLAG(&dmaOutHandle, DMA_FLAG_TCIF1_5))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaOutHandle, DMA_FLAG_TCIF1_5);
	}

	// Handle full transfer complete
	if (__HAL_DMA_GET_FLAG(&dmaOutHandle, DMA_FLAG_HTIF1_5))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaOutHandle, DMA_FLAG_HTIF1_5);
		xSemaphoreGiveFromISR(audioOutSemiphore, &xHigherPriorityTaskWoken);
	}

	// Handle transfer error 
	if (__HAL_DMA_GET_FLAG(&dmaOutHandle, DMA_FLAG_TEIF1_5))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaOutHandle, DMA_FLAG_TEIF1_5);
	}

	// FIFO error (?)
	if (__HAL_DMA_GET_FLAG(&dmaOutHandle, DMA_FLAG_FEIF1_5))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaOutHandle, DMA_FLAG_FEIF1_5);
	}
}