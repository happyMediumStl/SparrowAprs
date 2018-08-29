#ifndef NMEA_H
#define NMEA_H

#include <stm32f4xx_hal.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <stdint.h>

typedef struct
{
	uint8_t Hour;
	uint8_t Minute;
	float Second;
} NmeaTimeT;

typedef struct
{
	uint8_t Day;
	uint8_t Month;
	uint8_t Year;
} NmeaDateT;

typedef struct
{
	float Lat;
	float Lon;
} NmeaPositionT;

typedef struct
{
	NmeaTimeT Time;
	NmeaPositionT Position;
	uint8_t Fix;
	int32_t SatCount;
	float HDop;
	float Altitude;
	uint8_t AltitudeUnits;
} NmeaGgaT;

typedef struct
{
	NmeaTimeT Time;
	uint8_t Fix;
	NmeaPositionT Position;
	float Speed;
	float Track;
	NmeaDateT Date;
} NmeaRmcT;

typedef struct
{
	NmeaTimeT Time;
	uint8_t Day;
	uint8_t Month;
	uint32_t Year;
	uint8_t Valid;
} NmeaZdaT;

enum NMEA_MESSAGE_TYPE
{
	NMEA_MESSAGE_TYPE_GGA,
	NMEA_MESSAGE_TYPE_RMC,
	NMEA_MESSAGE_TYPE_GSA,
	NMEA_MESSAGE_TYPE_ZDA
};

typedef struct
{
	enum NMEA_MESSAGE_TYPE MessageType;
	union
	{
		NmeaGgaT Gga;
		NmeaRmcT Rmc;
		NmeaZdaT Zda;
	};
} GenericNmeaMessageT;

uint8_t Nmea0183Init(void);
void Nmea0183StartParser(void);
QueueHandle_t* Nmea0183GetQueue(void);
UART_HandleTypeDef* Nmea0183GetUartHandle(void);

#endif // !NMEA_H
