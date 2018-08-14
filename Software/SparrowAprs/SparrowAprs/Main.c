/*
	OZARK SPRING
		-- A Simple APRS encoder
		-- Chris Seto, 2018

	Special thanks:
		* Rob Ruark
		* Sterling Coffey

	APRS/Bell 202/AX25 Resources:
		* http://chapmanworld.com/2015/04/07/arduino-uno-and-fast-pwm-for-afsk1200/
		* https://hugosprojects.wordpress.com/2014/03/15/implementing-aprs/
		* https://hugosprojects.wordpress.com/2014/03/20/implementing-aprs-gps-data/
		* https://hugosprojects.wordpress.com/2014/03/25/implementing-aprs-message-at-the-bit-level/
		* http://tt7hab.blogspot.com/2017/02/aprs-automatic-packet-reporting-system.html
		* https://github.com/TomasTT7/TT7F-Float-Tracker/blob/master/Software/ARM_APRS.c
*/

#include <stm32f4xx_hal.h>
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include "Retarget.h"
#include "Audio.h"
#include "Dra818Io.h"
#include "Dra818.h"
#include "Beacon.h"
#include "Led.h"
#include "Nmea0183.h"
#include "Rtc.h"
#include "Radio.h"
#include "Bsp.h"
#include "FlashConfig.h"

static void SystemClock_Config(void);
void SystemIdle(void * pvParameters);

#define SYSTEM_IDLE_DELAY	100
static TaskHandle_t idleTaskHandle = NULL;

int main(void)
{
	// Init HAL
	HAL_Init();

	// Configure clock
	SystemClock_Config();

	// Init retarget
	RetargetInit();
	printf("%cBoard up!\r\n", 12);

	// Init config
	FlashConfigInit();

	printf("%lu\r\n", FlashConfigGetPtr()->Aprs.BeaconPeriod);

	// BSP init
	BspInit();

	// LED init
	LedInit();

	// RTC
	RtcInit();

	// Init DRA818
	Dra818Init();
	Dra818IoInit();

	// Init beacon
	BeaconInit();

	// Init Audio Out
	AudioInit();
	
	// Init NMEA
	Nmea0183Init();

	// Init radio
	RadioInit();

	// Start system idle
	xTaskCreate(SystemIdle,
		"SystemIdle",
		128,
		NULL,
		tskIDLE_PRIORITY,
		&idleTaskHandle);

	// Start nmea parser
	Nmea0183StartParser();

	// Start radio task
	RadioTaskStart();

	// Start beaconing
	BeaconStartTask();

	// Start RTOS kernal
	vTaskStartScheduler();

	// Do nothing
	while(1);
}

// System Idle task
void SystemIdle(void * pvParameters)
{
	while (1)
	{
		//printf("Alive\r\n");
		// Feed watchdog

		// Blink LED
		LedToggle(LED_1);

		// TODO: Do system health check

		// Wait
		vTaskDelay(SYSTEM_IDLE_DELAY / portTICK_PERIOD_MS);
	}
}

// Handle systick
void SysTick_Handler(void)
{
	HAL_IncTick();

	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
	{ 
		xPortSysTickHandler();
	}
}

// Configure clock system
static void SystemClock_Config(void)
{
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;

	// Enable clocks
	__HAL_RCC_PWR_CLK_ENABLE();
  
	/* The voltage scaling allows optimizing the power consumption when the device is 
	   clocked below the maximum system frequency, to update the voltage scaling value 
	   regarding system frequency refer to product datasheet.  */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  
	// HSE feeds PLL
	RCC_OscInitStruct.OscillatorType		= RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState				= RCC_HSE_BYPASS;
	RCC_OscInitStruct.PLL.PLLState			= RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource			= RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM				= 50;
	RCC_OscInitStruct.PLL.PLLN				= 336;
	RCC_OscInitStruct.PLL.PLLP				= RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ				= 7;
	
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		while (1)
		{
		}
	}

	// Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers
	RCC_ClkInitStruct.ClockType				= (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource			= RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider			= RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider		= RCC_HCLK_DIV4;  
	RCC_ClkInitStruct.APB2CLKDivider		= RCC_HCLK_DIV2;
	
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
	{
		while (1)
		{
		}
	}

	// Connect RTC to LSI
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_LSE;
	RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_NONE;
	RCC_OscInitStruct.LSEState       = RCC_LSE_OFF;
	RCC_OscInitStruct.LSIState       = RCC_LSI_ON;

	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		while (1)
		{
		}
	}

	__HAL_RCC_RTC_CLKPRESCALER(RCC_RTCCLKSOURCE_LSI);
	__HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSI);
	
	// STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported
	if(HAL_GetREVID() == 0x1001)
	{
		// Enable flash prefetch
		__HAL_FLASH_PREFETCH_BUFFER_ENABLE();
	}
}
