#include <stm32f4xx_hal.h>
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "Nmea0183.h"
#include "Rtc.h"
#include "time.h"
#include "UbloxNeo.h"
#include "GpsHub.h"
#include "Helpers.h"

// Max difference between RTC and GPS time allowed in S
#define MAX_TIME_DELTA			5

// Max GPS packet timeout before we reconfigure in mS
#define MAX_GPS_TIMEOUT			2500
#define GPS_RECONFIGURE_TIMEOUT	5000

#define GPS_EPOCH				1900

// Frequency of situation updates
#define SITUATION_UPDATE		1000

typedef struct
{
	float Lat;
	float Lon;
	float Altitude;
	float LastAltitude;
	float Track;
	float LastTrack;
	float Speed;
	float LastSpeed;
} InternalSituationInfoT;

static QueueHandle_t* situationQueue;

static void GpsHubTask(void* pvParameters);
static TaskHandle_t gpsHubTaskHandle = NULL;

void GpsHubInit(void)
{
	situationQueue = xQueueCreate(1, sizeof(SituationInfoT));

	if (situationQueue == NULL)
	{
		return;
	}
}

void GpsHubStartTask(void)
{
	// Create task
	xTaskCreate(GpsHubTask,
		"GpsHub",
		300,
		NULL,
		5,
		&gpsHubTaskHandle);
}

void HandleNewZda(const NmeaZdaT* zda)
{
	struct tm time;
	uint32_t gpsTimeStamp;
	uint32_t currentTime;

	// Ignore if we have no fix
	if (!zda->Valid || zda->Year == 0)
	{
		return;
	}

	// Get the current time
	currentTime = RtcGet();
	
	// Load the time structure with GPS fields
	time.tm_hour = zda->Time.Hour;
	time.tm_min  = zda->Time.Minute;
	time.tm_sec  = zda->Time.Second;
	time.tm_year = (zda->Year - GPS_EPOCH);
	time.tm_mon  = (zda->Month - 1);
	time.tm_mday = zda->Day;

	// Make timestamp from GPS time
	gpsTimeStamp = (uint32_t)mktime(&time);

	// If there is more than 5 second difference between our system time and GPS
	// Because of uint32_t arithmatic, one of these comparsions will always be true if one value is larger than the other
	// If both are true, it means one value is larger than the other, and that difference is more than MAX_TIME_DELTA
	if (currentTime - gpsTimeStamp >= MAX_TIME_DELTA && gpsTimeStamp - currentTime >= MAX_TIME_DELTA)
	{
		// Set the new system time
		RtcSet(gpsTimeStamp);
		printf("Set RTC\r\n");
	}
}

void GpsHubGetSituation(SituationInfoT* situation)
{
	xQueuePeek(situationQueue, situation, 0);
}

static void GpsHubTask(void* pvParameters)
{
	GenericNmeaMessageT msg;
	QueueHandle_t* nmeaQueue;
	TickType_t lastTaskTime = 0;
	TickType_t lastSituationUpdateTime = 0;

	TickType_t lastCheckForTimeoutTime = 0;
	TickType_t lastGga = 0;
	TickType_t lastZda = 0;
	TickType_t lastRmc = 0;
	TickType_t timeNow;

	InternalSituationInfoT situation;
	SituationInfoT beaconInfo;

	// Get GPS NMEA queue
	nmeaQueue = Nmea0183GetQueue();

	// Something has gone wrong if we never get this
	if (nmeaQueue == NULL)
	{
		return;
	}

	// Task loop
	while (1)
	{
		// Block until it's time to start
		vTaskDelayUntil(&lastTaskTime, 100);

		timeNow = xTaskGetTickCount();

		// Try to get a new message from NMEA as long as there is data to read
		if(!xQueueIsQueueEmptyFromISR(nmeaQueue))
		{
			// Fetch the message
			if(xQueueReceive(nmeaQueue, &msg, 0))
			{
				// Handle messages
				switch(msg.MessageType)
				{
					// Handle GGA message
					case NMEA_MESSAGE_TYPE_GGA :
						// Mark that we did get a packet
						lastGga = timeNow;

						// Ignore if we have no fix or if the fix is not present
						if (!IsAscii(msg.Gga.Fix) || msg.Gga.Fix <= '0')
						{
							continue;
						}

						situation.Lat = msg.Gga.Position.Lat;
						situation.Lon = msg.Gga.Position.Lon;
						situation.Altitude = msg.Gga.Altitude;
						break;

					// Handle RMC message
					case NMEA_MESSAGE_TYPE_RMC :
						// Mark that we did get a packet
						lastRmc = timeNow;

						// Ignore if we have no fix
						if (msg.Rmc.Fix != 'A')
						{
							continue;
						}

						// Exact from the new GPS packet
						situation.LastSpeed = situation.Speed;
						situation.Speed = msg.Rmc.Speed;
						situation.LastTrack = situation.Track;
						situation.Track = msg.Rmc.Track;
						break;
					
					// Handle ZDA message
					case NMEA_MESSAGE_TYPE_ZDA :
						// Mark that we did get a packet
						lastZda = timeNow;
						HandleNewZda(&msg.Zda);
						break;

				default:
				case NMEA_MESSAGE_TYPE_GSA:
					break;
				}
			}
		}

		// Check for GPS timeouts on packets
		if (timeNow - lastCheckForTimeoutTime > GPS_RECONFIGURE_TIMEOUT)
		{
			lastCheckForTimeoutTime = timeNow;

			if (timeNow - lastGga > MAX_GPS_TIMEOUT)
			{
				UbloxNeoSetOutputRate("GGA", 1);
			}

			if (timeNow - lastRmc > MAX_GPS_TIMEOUT)
			{
				UbloxNeoSetOutputRate("RMC", 1);
			}

			if (timeNow - lastZda > MAX_GPS_TIMEOUT)
			{
				UbloxNeoSetOutputRate("ZDA", 2);
			}
		}

		// 1hz updates to situation
		if (timeNow - lastSituationUpdateTime > SITUATION_UPDATE)
		{
			// Fill out situation info
			beaconInfo.Lat = situation.Lat;
			beaconInfo.Lon = situation.Lon;
			beaconInfo.Altitude = situation.Altitude;
			beaconInfo.dAltitude = situation.Altitude - situation.LastAltitude;
			beaconInfo.Speed = situation.Speed;
			beaconInfo.dSpeed = situation.Speed - situation.LastSpeed;
			beaconInfo.Track = situation.Track;
			beaconInfo.dTrack = situation.Track - situation.LastTrack;

			// Enqueue
			xQueueOverwrite(situationQueue, &beaconInfo);
		}
	}
}