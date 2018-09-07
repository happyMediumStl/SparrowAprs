#include <stm32f4xx_hal.h>
#include <math.h>
#include <string.h>
#include <time.h>

// Stolen from:
// https://community.st.com/thread/46986-unix-epoch-timestamp-to-rtc?commentID=183088

#define RTC_TIME_GOOD_WORD		0x32F2

// Peripheral handles
static RTC_HandleTypeDef hrtc;

// 1 Mhz clock provided by HSE_DIV25
#define RTC_PREDIV_A	125
#define RTC_PREDIV_S	8000

// Unix epoch information
// STM32 cares about 
#define MONTH_EPOCH	1
#define YEAR_EPOCH	(2000 - 1900)

void RtcInit(void)
{
	// Enable clocks
	__HAL_RCC_RTC_ENABLE();

	// Configure RTC
	hrtc.Instance = RTC;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
	hrtc.Init.AsynchPrediv = RTC_PREDIV_A - 1;
	hrtc.Init.SynchPrediv = RTC_PREDIV_S - 1;
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	
	// Init RTC
	HAL_RTC_Init(&hrtc);
}

// Set RTC by TS
void RtcSet(const uint32_t now)
{
	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;
	struct tm time_tm;
	time_tm = *localtime((time_t*)&now);

	// Configure time
	sTime.Hours = (uint8_t)time_tm.tm_hour;
	sTime.Minutes = (uint8_t)time_tm.tm_min;
	sTime.Seconds = (uint8_t)time_tm.tm_sec;
	
	if (time_tm.tm_wday == 0)
	{
		time_tm.tm_wday = 7;
	}

	// Configure date
	sDate.WeekDay = (uint8_t)time_tm.tm_wday;
	sDate.Month = (uint8_t)time_tm.tm_mon + MONTH_EPOCH;
	sDate.Date = (uint8_t)time_tm.tm_mday;
	sDate.Year = (uint16_t)(time_tm.tm_year + 1900 - 2000);

	// Set RTC
	HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

	// Indicate that the RTC now contains a valid time
	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_TIME_GOOD_WORD);
}

// Get timestamp from RTC
uint32_t RtcGet(void)
{
	RTC_DateTypeDef rtcDate;
	RTC_TimeTypeDef rtcTime;
	struct tm tim;

	// Check if the RTC contains a valid time
	if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != RTC_TIME_GOOD_WORD)
	{
		return 0;
	}

	// Get time/date
	HAL_RTC_GetTime(&hrtc, &rtcTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &rtcDate, RTC_FORMAT_BIN);

	// Configure the struct
	tim.tm_year = (uint16_t)(rtcDate.Year + 2000 - 1900);
	tim.tm_mon  = rtcDate.Month - MONTH_EPOCH;
	tim.tm_mday = rtcDate.Date;
	tim.tm_hour = rtcTime.Hours;
	tim.tm_min  = rtcTime.Minutes;
	tim.tm_sec  = rtcTime.Seconds;

	return  mktime(&tim);
}