#include <stm32f4xx_hal.h>
#include "FreeRTOS.h"
#include "task.h"
#include "Retarget.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "Ax25.h"
#include "Dra818.h"
#include "Dra818Io.h"
#include "Audio.h"
#include "Afsk.h"
#include "Aprs.h"
#include "Beacon.h"
#include "Led.h"
#include "Nmea0183.h"
#include "Rtc.h"
#include "time.h"
#include "Radio.h"
#include "Config.h"
#include "FlashConfig.h"

#define MAX_TIME_DELTA	5

#define PACKET_BUFFER_SIZE		500
static uint8_t aprsBuffer[PACKET_BUFFER_SIZE];

void BeaconTask(void* pvParameters);
static TaskHandle_t beaconTaskHandle = NULL;

static ConfigT* config;

struct
{
	NmeaPositionT Position;
	float Altitude;
	float Speed;
	float Track;
} beaconInfo;

void BeaconInit(void)
{
	config = FlashConfigGetPtr(); 
}

void BeaconStartTask(void)
{
	// Create task
	xTaskCreate(BeaconTask,
		"Beacon",
		512,
		NULL,
		2,
		&beaconTaskHandle);
}

void HandleNewGga(const NmeaGgaT* gga)
{
	// If there is no fix, ignore it
	if (gga->Fix == '0')
	{
		return;
	}

	// Extract the location
	beaconInfo.Position.Lat = gga->Position.Lat;
	beaconInfo.Position.Lon = gga->Position.Lon;
	beaconInfo.Altitude = gga->Altitude;
}

void HandleNewRmc(const NmeaRmcT* rmc)
{
	// Ignore if we have no fix
	if (rmc->Fix != 'A')
	{
		return;
	}

	// Exact from the new GPS packet
	beaconInfo.Speed = rmc->Speed;
	beaconInfo.Track = rmc->Track;
}

void HandleNewZda(const NmeaZdaT* zda)
{
	struct tm time;
	time_t gpsTimeStamp;
	uint32_t currentTime;

	// Only act if the packet is valid
	if (!zda->Valid)
	{
		return;
	}

	// Get the current time
	currentTime = RtcGet();
	
	// Load the time structure with GPS fields
	time.tm_hour = zda->Time.Hour;
	time.tm_min  = zda->Time.Minute;
	time.tm_sec  = zda->Time.Second;
	time.tm_year = zda->Year;
	time.tm_mon  = zda->Month;
	time.tm_mday = zda->Day;

	// Make timestamp from GPS time
	gpsTimeStamp = mktime(&time);

	// If there is more than 5 second difference between our system time and GPS
	// Because of uint32_t arithmatic, one of these comparsions will always be true if one value is larger than the other
	// If both are true, it means one value is larger than the other, and that difference is more than MAX_TIME_DELTA
	if (currentTime - gpsTimeStamp > MAX_TIME_DELTA && gpsTimeStamp - currentTime > MAX_TIME_DELTA)
	{
		// Set the new system time
		RtcSet(gpsTimeStamp);
		printf("Time set!\r\n %lu", gpsTimeStamp);
	}
}

static float Meters2Feet(const float m)
{
	// 10000 ft = 3048 m
	return m / 0.3048;
}

void BeaconTask(void* pvParameters)
{
	GenericNmeaMessageT msg;
	QueueHandle_t* nmeaQueue;
	QueueHandle_t* txQueue;
	TickType_t lastTaskTime = 0;
	TickType_t lastBeaconTime = 0;
	uint32_t aprsLength;
	AprsPositionReportT aprsReport;
	RadioPacketT beaconPacket;
	uint32_t beaconPeriod;

	// Get the queues
	nmeaQueue = Nmea0183GetQueue();
	txQueue = RadioGetTxQueue();

	// Location beacon loop
	while (1)
	{
		// Block until it's time to start
		vTaskDelayUntil(&lastTaskTime, 10);

		// Try to get a new message from NMEA as long as there is data to read
		if (!xQueueIsQueueEmptyFromISR(nmeaQueue))
		{
			// Fetch the message
			if (xQueueReceive(nmeaQueue, &msg, 0))
			{
				// Handle messages
				switch (msg.MessageType)
				{
					// Handle GGA message
					case NMEA_MESSAGE_TYPE_GGA:
						HandleNewGga(&msg.Gga);
						break;

					// Handle RMC message
					case NMEA_MESSAGE_TYPE_RMC :
						HandleNewRmc(&msg.Rmc);
						break;
					
					// Handle ZDA message
					case NMEA_MESSAGE_TYPE_ZDA:
						HandleNewZda(&msg.Zda);
						break;

					default:
					case NMEA_MESSAGE_TYPE_GSA:
						break;
				}
			}
		}

		// Compute smart beacon period
		if (config->Aprs.UseSmartBeacon)
		{
			// TODO: Smart beacon
			beaconPeriod = config->Aprs.BeaconPeriod;
		}
		else
		{
			beaconPeriod = config->Aprs.BeaconPeriod;
		}

		// If it's time to beacon, assemble the report and send it
		if(xTaskGetTickCount() - lastBeaconTime > beaconPeriod)
		{
			lastBeaconTime = xTaskGetTickCount();
			printf("Beaconing: %f, %f\r\n", beaconInfo.Position.Lat, beaconInfo.Position.Lon);
	
			// Configure Ax25 frame
			memcpy(beaconPacket.Frame.Source, config->Aprs.Callsign, 6);
			beaconPacket.Frame.SourceSsid = config->Aprs.Ssid;
			memcpy(beaconPacket.Frame.Destination, "APRS  ", 6);
			beaconPacket.Frame.DestinationSsid = 0;
			memcpy(beaconPacket.Path, config->Aprs.Path, 7);
			beaconPacket.Frame.PathLen = 7;
			beaconPacket.Frame.PreFlagCount = 10;
			beaconPacket.Frame.PostFlagCount = 10;

			// Fill APRS report
			aprsReport.Timestamp = RtcGet();
			aprsReport.Position.Lat = beaconInfo.Position.Lat;
			aprsReport.Position.Lon = beaconInfo.Position.Lon;
			aprsReport.Position.Symbol = config->Aprs.Symbol;
			aprsReport.Position.SymbolTable = config->Aprs.SymbolTable;

			// Build APRS report
			aprsLength = 0;
			aprsLength += AprsMakePosition(aprsBuffer, &aprsReport);
			aprsLength += AprsMakeExtCourseSpeed(aprsBuffer + aprsLength, (uint8_t)beaconInfo.Track, (uint16_t)beaconInfo.Speed);

			// Append comment for altitude
			aprsLength += snprintf((char*)aprsBuffer + aprsLength, 8, "A=%06i", (int32_t)Meters2Feet(beaconInfo.Altitude));

			// Add the APRS report to the packet
			memcpy(beaconPacket.Payload, aprsBuffer, aprsLength);
			beaconPacket.Frame.PayloadLength = aprsLength;

			// Enqueue the packet in the transmit buffer
			xQueueSendToBack(txQueue, &beaconPacket, 0);
		}
	}
}