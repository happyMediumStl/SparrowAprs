#include <stm32f4xx_hal.h>
#include "Bsp.h"

// Peripheral handles
static DMA_HandleTypeDef dmaHandle;
static ADC_HandleTypeDef adcHandle;

// DMA Sample buffer
#define ADC_SAMPLE_BUFFER_SIZE		32
#define TEMP_COUNT					16
#define VSENSE_COUNT				16
static volatile uint16_t bspAdcBuffer[ADC_SAMPLE_BUFFER_SIZE];

// Input voltage sense
static volatile uint32_t vSenseAccumulator = 0;
static volatile uint32_t tSenseAccumulator = 0;

// ADC calibration slope. Store it so we only calculate it once
static float tSlope = 0;

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

// Ticks to volts
#define TICKS_TO_VOLTS			0.000805860805860f

// Temperature sensor trimming values
// Defined DocID022152 Rev 8 page 138
#define TS_CAL1_T		30.0f
#define TS_CAL2_T		110.0f
#define TS_CAL1			(uint16_t*)(0x1FFF7A2C)
#define TS_CAL2			(uint16_t*)(0x1FFF7A2E)

// Init BSP peripherals
void BspInit(void)
{
	// Pa1 Vsense
	GPIO_InitTypeDef GPIO_InitStruct;
	ADC_ChannelConfTypeDef sConfig;

	// Precalculate the temperature curve slope
	tSlope = (TS_CAL2_T - TS_CAL1_T) / (*TS_CAL2 - *TS_CAL1);

	// Enable Clocks
	__GPIOA_CLK_ENABLE();
	__ADC1_CLK_ENABLE();
	__DMA2_CLK_ENABLE();

	// Configure ADC output pin
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;

	// PA1 ADC1_IN1
	GPIO_InitStruct.Pin = GPIO_PIN_1;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// Configure ADC
	adcHandle.Instance					 = ADC1;
	adcHandle.Init.ClockPrescaler        = ADC_CLOCKPRESCALER_PCLK_DIV8;
	adcHandle.Init.Resolution            = ADC_RESOLUTION_12B;
	adcHandle.Init.ScanConvMode          = ENABLE;
	adcHandle.Init.ContinuousConvMode    = ENABLE;
	adcHandle.Init.DiscontinuousConvMode = DISABLE;
	adcHandle.Init.NbrOfDiscConversion   = 0;
	adcHandle.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIG_EDGE_NONE;
	adcHandle.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
	adcHandle.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
	adcHandle.Init.NbrOfConversion       = 2;
	adcHandle.Init.DMAContinuousRequests = DISABLE;
	adcHandle.Init.EOCSelection          = DISABLE;
	HAL_ADC_Init(&adcHandle);

	// Configure in channels
	sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
	sConfig.Offset       = 0;

	// V+ VSense
	sConfig.Channel      = ADC_CHANNEL_1;
	sConfig.Rank         = 1;
	HAL_ADC_ConfigChannel(&adcHandle, &sConfig);

	// uC temperature sensor
	// HAL automatically configures the temperature sensor
	sConfig.Channel      = ADC_CHANNEL_16;
	sConfig.Rank         = 2;
	HAL_ADC_ConfigChannel(&adcHandle, &sConfig);

	// Configure DMA
	// ADC1 DMA2 Stream4, channel 0
	dmaHandle.Instance					= DMA2_Stream4;
	dmaHandle.Init.Channel				= DMA_CHANNEL_0;
	dmaHandle.Init.Direction			= DMA_PERIPH_TO_MEMORY;
	dmaHandle.Init.PeriphInc			= DMA_PINC_DISABLE;
	dmaHandle.Init.MemInc				= DMA_MINC_ENABLE;
	dmaHandle.Init.PeriphDataAlignment	= DMA_PDATAALIGN_HALFWORD;
	dmaHandle.Init.MemDataAlignment		= DMA_MDATAALIGN_HALFWORD;
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
	HAL_ADC_Start_DMA(&adcHandle, (uint32_t*)bspAdcBuffer, ADC_SAMPLE_BUFFER_SIZE);
}

// Process raw data from the ADC DMA buffer
static void ConvertBspMeasurments(void)
{
	uint32_t tempAccumulator = 0;
	uint32_t i = 0;

	// Extract VSense samples
	for (i = 0; i < VSENSE_COUNT; i++)
	{
		tempAccumulator += bspAdcBuffer[(i * 2) + 0];
	}

	vSenseAccumulator = tempAccumulator;
	tempAccumulator = 0;

	// Extract onboard temperature sensor values
	for (i = 0; i < TEMP_COUNT; i++)
	{
		tempAccumulator += bspAdcBuffer[(i * 2) + 1];
	}

	tSenseAccumulator = tempAccumulator;
	tempAccumulator = 0;
}

// Handle Audio Out DMA
void DMA2_Stream4_IRQHandler(void)
{
	// Handle transfer complete
	if(__HAL_DMA_GET_FLAG(&dmaHandle, DMA_FLAG_TCIF0_4))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaHandle, DMA_FLAG_TCIF0_4);

		// Transfer is complete, process the samples
		ConvertBspMeasurments();

		// Start the next round of conversions
		HAL_ADC_Stop_DMA(&adcHandle);
		HAL_ADC_Start_DMA(&adcHandle, (uint32_t*)bspAdcBuffer, ADC_SAMPLE_BUFFER_SIZE);
	}

	// Handle half transfer complete
	if(__HAL_DMA_GET_FLAG(&dmaHandle, DMA_FLAG_HTIF0_4))
	{
		__HAL_DMA_CLEAR_FLAG(&dmaHandle, DMA_FLAG_HTIF0_4);
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
	return ((float)vSenseAccumulator / VSENSE_COUNT) * VIN_TICKS_TO_VOLTS;
}

// Get the internal chip temperature in C
float BspGetuCTemperature(void)
{
	/*
		Ref:
			* DocID022152 Rev 8, page 39, 138
			* DocID022101 Rev 3

		The temperature sensor in the STM32 is a low accuracy device,
		though it can be calibrated using TS_CAL1 and TS_CAL2
	*/

	// Calculate temperature
	return (tSlope * (((float)tSenseAccumulator / (float)TEMP_COUNT) - *TS_CAL1)) + TS_CAL1_T;
}