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

#define MAX_TIME_DELTA	5

#define PACKET_BUFFER_SIZE		500
static uint8_t aprsBuffer[PACKET_BUFFER_SIZE];

void BeaconTask(void* pvParameters);
static TaskHandle_t beaconTaskHandle = NULL;

struct
{
	NmeaPositionT Position;
} beaconInfo;

void BeaconInit(void)
{
}

// Configure AX25 frame
static void ConfigureAx25Frame(void)
{

}

// Send a position report
static void SendReport(void)
{

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
}

void HandleNewRmc(const NmeaRmcT* rmc)
{
	struct tm time;
	time_t gpsTimeStamp;

	// If there is no fix, ignore it
	if (rmc->Fix != 'A')
	{
		return;
	}

	// Create a timestamp from the GPS timedate
	time.tm_hour = rmc->Time.Hour;
	time.tm_min  = rmc->Time.Minute;
	time.tm_sec  = rmc->Time.Second;
	time.tm_year = rmc->Date.Year - 1900;
	time.tm_mon  = rmc->Date.Month - 1;
	time.tm_mday = rmc->Date.Day;

	// Make timestamp from GPS time
	gpsTimeStamp = mktime(&time);

	// If there is more than 5 second difference between our system time and GPS
	if(RtcGet() - gpsTimeStamp > MAX_TIME_DELTA)
	{
		// Set the new system time
		RtcSet(gpsTimeStamp);

		printf("Time set!\r\n %lu", gpsTimeStamp);
	}
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

	printf("Beacon up!\r\n");

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
					case NMEA_MESSAGE_TYPE_GGA :
						printf("GGA");
						HandleNewGga(&msg.Gga);
						break;

					// Handle RMC message
					case NMEA_MESSAGE_TYPE_RMC :
						printf("RMC");
						HandleNewRmc(&msg.Rmc);
						break;

					case NMEA_MESSAGE_TYPE_ZDA:
						printf("ZDT");
						break;

					default:
					case NMEA_MESSAGE_TYPE_GSA:
						break;
				}
			}
		}

		// If it's time to beacon, assemble the report and send it
		if (xTaskGetTickCount() - lastBeaconTime > 45000)
		{
			lastBeaconTime = xTaskGetTickCount();
			printf("Beaconing: %f, %f\r\n", beaconInfo.Position.Lat, beaconInfo.Position.Lon);
	
			// Configure Ax25 frame
			memcpy(beaconPacket.Frame.Source, "KD0POQ", 6);
			beaconPacket.Frame.SourceSsid = 11;
			memcpy(beaconPacket.Frame.Destination, "APRS  ", 6);
			beaconPacket.Frame.DestinationSsid = 0;
			memcpy(beaconPacket.Path, (uint8_t*)"WIDE2 1", 7);
			beaconPacket.Frame.PathLen = 7;
			beaconPacket.Frame.PreFlagCount = 10;
			beaconPacket.Frame.PostFlagCount = 10;

			// Fill APRS report
			aprsReport.Comment = (uint8_t*)"ChrisAprs";
			aprsReport.CommentLength = 9;
			aprsReport.Timestamp = RtcGet();
			aprsReport.Position.Lat = beaconInfo.Position.Lat;
			aprsReport.Position.Lon = beaconInfo.Position.Lon;
			aprsReport.Position.Symbol = 'O';
			aprsReport.Position.SymbolTable = '/';

			// Build APRS report
			aprsLength = AprsMakePosition(aprsBuffer, &aprsReport);

			// Add the APRS report to the packet
			memcpy(beaconPacket.Payload, aprsBuffer, aprsLength);
			beaconPacket.Frame.PayloadLength = aprsLength;

			// Enqueue the packet in the transmit buffer
			xQueueSendToBack(txQueue, &beaconPacket, 0);
		}
	}
}