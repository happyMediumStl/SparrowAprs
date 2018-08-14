#include <stm32f4xx_hal.h>

// Peripheral handles
static DMA_HandleTypeDef dmaHandle;
static ADC_HandleTypeDef adcHandle;

// DMA Sample buffer
#define BSP_ADC_SAMPLE_BUFFER_SIZE		32
static uint16_t bspAdcBuffer[BSP_ADC_SAMPLE_BUFFER_SIZE];

// Input voltage sense
static uint32_t VSenseAccumulator = 0;

/*
	Vin ticks to voltage

	Vin Vdiv:
	R1 = 46.4K
	R2 = 10K

	Full equation:
	(3.3*ticks)/4095 = Vin*(10/10+46.4)

	Vin to ticks: Vin*220.01934236f
	Ticks to vin: Ticks*0.004545054945054945f
*/
#define VIN_TICKS_TO_VOLTS		0.004545054945054945f

// Init BSP peripherals
void BspInit(void)
{
	// Pa1 Vsense
	GPIO_InitTypeDef GPIO_InitStruct;
	ADC_ChannelConfTypeDef sConfig;

	// Enable Clocks
	__GPIOA_CLK_ENABLE();
	__ADC1_CLK_ENABLE();
	__DMA2_CLK_ENABLE();

	// Configure ADC output pin
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;

	// PA1 ADC1_IN1
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Configure ADC
	adcHandle.Instance					 = ADC1;
	adcHandle.Init.ClockPrescaler        = ADC_CLOCKPRESCALER_PCLK_DIV4;
	adcHandle.Init.Resolution            = ADC_RESOLUTION_12B;
	adcHandle.Init.ScanConvMode          = DISABLE;
	adcHandle.Init.ContinuousConvMode    = ENABLE;
	adcHandle.Init.DiscontinuousConvMode = DISABLE;
	adcHandle.Init.NbrOfDiscConversion   = 0;
	adcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIG_EDGE_NONE;
	adcHandle.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T8_TRGO;
	adcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
	adcHandle.Init.NbrOfConversion       = 1;
	adcHandle.Init.DMAContinuousRequests = ENABLE;
	adcHandle.Init.EOCSelection          = DISABLE;
	HAL_ADC_Init(&adcHandle);

	// Configure in channel
	sConfig.Rank         = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
	sConfig.Offset       = 0;

	// V+ VSense
	sConfig.Channel      = ADC_CHANNEL_1;
	HAL_ADC_ConfigChannel(&adcHandle, &sConfig);

	// Configure DMA
	// ADC1 DMA2 Stream4, channel 0
	dmaHandle.Instance					= DMA2_Stream4;
	dmaHandle.Init.Channel				= DMA_CHANNEL_0;
	dmaHandle.Init.Direction			= DMA_PERIPH_TO_MEMORY;
	dmaHandle.Init.PeriphInc			= DMA_PINC_DISABLE;
	dmaHandle.Init.MemInc				= DMA_MINC_ENABLE;
	dmaHandle.Init.PeriphDataAlignment	= DMA_PDATAALIGN_BYTE;
	dmaHandle.Init.MemDataAlignment		= DMA_MDATAALIGN_BYTE;
	dmaHandle.Init.Mode					= DMA_NORMAL;
	dmaHandle.Init.Priority				= DMA_PRIORITY_HIGH;
	dmaHandle.Init.FIFOMode				= DMA_FIFOMODE_DISABLE;         
	dmaHandle.Init.FIFOThreshold		= DMA_FIFO_THRESHOLD_HALFFULL;
	dmaHandle.Init.MemBurst				= DMA_MBURST_SINGLE;
	dmaHandle.Init.PeriphBurst			= DMA_PBURST_SINGLE; 
	HAL_DMA_Init(&dmaHandle);

	__HAL_LINKDMA(&adcHandle, DMA_Handle, dmaHandle);

	// Enable interrupts
	HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, 0xf, 0xf);
	HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);

	// Start measuring
	HAL_ADC_Start_DMA(&adcHandle, (uint32_t*)bspAdcBuffer, BSP_ADC_SAMPLE_BUFFER_SIZE);
}

static void ConvertBspMeasurments(void)
{
	uint32_t tempAccumulator = 0;
	uint32_t i = 0;

	for (i = 0; i < BSP_ADC_SAMPLE_BUFFER_SIZE; i++)
	{
		tempAccumulator += bspAdcBuffer[i];
	}

	VSenseAccumulator = tempAccumulator;
}

// Handle Audio Out DMA
void DMA2_Stream4_IRQHandler(void)
{
	// Handle half transfer
	if(__HAL_DMA_GET_FLAG(&dmaHandle, DMA_FLAG_TCIF0_4))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaHandle, DMA_FLAG_TCIF0_4);
	}

	// Handle full transfer complete
	if(__HAL_DMA_GET_FLAG(&dmaHandle, DMA_FLAG_HTIF0_4))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaHandle, DMA_FLAG_HTIF0_4);

		// Transfer is complete, process the samples
		ConvertBspMeasurments();

		// Start the next round of conversions
		HAL_ADC_Start_DMA(&adcHandle, (uint32_t*)bspAdcBuffer, BSP_ADC_SAMPLE_BUFFER_SIZE);
	}

	// Handle transfer error 
	if(__HAL_DMA_GET_FLAG(&dmaHandle, DMA_FLAG_TEIF0_4))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaHandle, DMA_FLAG_TEIF0_4);
	}

	// FIFO error
	if(__HAL_DMA_GET_FLAG(&dmaHandle, DMA_FLAG_FEIF0_4))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaHandle, DMA_FLAG_FEIF0_4);
	}
}

// Get V in voltage in volts
float BspGetVSense(void)
{
	return ((float)VSenseAccumulator / BSP_ADC_SAMPLE_BUFFER_SIZE) * VIN_TICKS_TO_VOLTS;
}