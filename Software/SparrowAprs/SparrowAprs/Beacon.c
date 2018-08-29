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
#include "Bme280Shim.h"
#include "Bsp.h"
#include "UbloxNeo.h"

// Max difference between RTC and GPS time allowed in S
#define MAX_TIME_DELTA			5

// Max GPS packet timeout before we reconfigure in mS
#define MAX_GPS_TIMEOUT			5000
#define GPS_RECONFIGURE_TIMEOUT	5000

// APRS packet buffer
#define PACKET_BUFFER_SIZE		500
static uint8_t aprsBuffer[PACKET_BUFFER_SIZE];

void BeaconTask(void* pvParameters);
static TaskHandle_t beaconTaskHandle = NULL;

static TickType_t lastGga = 0;
static TickType_t lastZda = 0;
static TickType_t lastRmc = 0;

struct
{
	NmeaPositionT Position;
	float Altitude;
	float Speed;
	float Track;
	float LastTrack;
	float LastSpeed;
} beaconInfo;

void BeaconInit(void)
{
}

void BeaconStartTask(void)
{
	// Create task
	xTaskCreate(BeaconTask,
		"Beacon",
		512,
		NULL,
		3,
		&beaconTaskHandle);
}

void HandleNewGga(const NmeaGgaT* gga)
{
	// Mark that we did get a packet
	lastGga = xTaskGetTickCount();

	// Ignore if we have no fix
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
	// Mark that we did get a packet
	lastRmc = xTaskGetTickCount();

	// Ignore if we have no fix
	if (rmc->Fix != 'A')
	{
		return;
	}

	// Exact from the new GPS packet
	beaconInfo.LastSpeed = beaconInfo.Speed;
	beaconInfo.Speed = rmc->Speed;
	beaconInfo.LastTrack = beaconInfo.Track;
	beaconInfo.Track = rmc->Track;
}

void HandleNewZda(const NmeaZdaT* zda)
{
	struct tm time;
	time_t gpsTimeStamp;
	uint32_t currentTime;

	// Mark that we did get a packet
	lastZda = xTaskGetTickCount();

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
	time.tm_year = (zda->Year - 1900);
	time.tm_mon  = (zda->Month - 1);
	time.tm_mday = zda->Day;

	// Make timestamp from GPS time
	gpsTimeStamp = mktime(&time);

	printf("(RTC %li) (GPS %li)\r\n", RtcGet(), gpsTimeStamp);

	// If there is more than 5 second difference between our system time and GPS
	// Because of uint32_t arithmatic, one of these comparsions will always be true if one value is larger than the other
	// If both are true, it means one value is larger than the other, and that difference is more than MAX_TIME_DELTA
	if (currentTime - gpsTimeStamp > MAX_TIME_DELTA && gpsTimeStamp - currentTime > MAX_TIME_DELTA)
	{
		// Set the new system time
		RtcSet(gpsTimeStamp);
	}
}

#define METERS_TO_FEET	3.28084f
static float Meters2Feet(const float m)
{
	return m * METERS_TO_FEET;
}

#define SB_HEADING_SCALE	.5f
#define SB_SPEED_SCALE		.5f

static uint8_t CalculateSmartBeacon(const uint16_t defaultTime)
{
	uint16_t time;
	float dHeading = 0;
	float dSpeed = 0;

	// Start with the normal beacon time
	time = defaultTime;

	// Get the derivative of heading and speed
	dHeading = fabs(beaconInfo.Track - beaconInfo.LastTrack);
	dSpeed = fabs(beaconInfo.Speed - beaconInfo.LastSpeed);

	// The greater our dH or dS, the less our beacontime is


	// Get our speed
	return 0;
}

static uint8_t GpsCheckTimeOut(void)
{
	TickType_t timeNow = xTaskGetTickCount();

	if (timeNow - lastGga > MAX_GPS_TIMEOUT)
	{
		return 1;
	}

	if (timeNow - lastRmc > MAX_GPS_TIMEOUT)
	{
		return 1;
	}

	if (timeNow - lastZda > MAX_GPS_TIMEOUT)
	{
		return 1;
	}

	return 0;
}

static void GpsConfigure(void)
{
	UbloxNeoSetOutputRate("ZDA", 2);
	UbloxNeoSetOutputRate("RMC", 1);
	UbloxNeoSetOutputRate("GGA", 1);
}

void BeaconTask(void* pvParameters)
{
	GenericNmeaMessageT msg;
	QueueHandle_t* nmeaQueue;
	QueueHandle_t* txQueue;
	TickType_t lastTaskTime = 0;
	TickType_t lastBeaconTime = 0;
	TickType_t lastCheckForTimeoutTime = 0;
	uint32_t aprsLength;
	AprsPositionReportT aprsReport;
	RadioPacketT beaconPacket;
	uint32_t beaconPeriod;
	float temperature;
	float pressure;
	float humidity;

	// Get config
	ConfigT* config = FlashConfigGetPtr(); 

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
						HandleNewGga(&msg.Gga);
						break;

					// Handle RMC message
					case NMEA_MESSAGE_TYPE_RMC :
						HandleNewRmc(&msg.Rmc);
						break;
					
					// Handle ZDA message
					case NMEA_MESSAGE_TYPE_ZDA :
						HandleNewZda(&msg.Zda);
						break;

					default:
					case NMEA_MESSAGE_TYPE_GSA:
						break;
				}
			}
		}

		// Check for GPS timeouts
		if(xTaskGetTickCount() - lastCheckForTimeoutTime > GPS_RECONFIGURE_TIMEOUT)
		{
			lastCheckForTimeoutTime = xTaskGetTickCount();

			// If there is a timeout, we'll need to reconfigure
			if (GpsCheckTimeOut())
			{
				GpsConfigure();
			}
		}

		// Compute smart beacon period
		if (config->Aprs.UseSmartBeacon)
		{
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
			beaconPacket.Frame.PreFlagCount = 80;
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

			// Append comment for GPS altitude
			aprsLength += sprintf((char*)aprsBuffer + aprsLength, "A=%06i", (int)Meters2Feet(beaconInfo.Altitude)) - 1;

			// Get the current environmental data
			Bme280ShimGetTph(&temperature, &pressure, &humidity);

			// Add telemetry comment
			aprsLength += sprintf((char*)aprsBuffer + aprsLength, "/Pa=%06i/Rh=%02.02f/Ti=%02.02f/V=%02.02f", (int)pressure, humidity, temperature, BspGetVSense()) - 1;

			// Add the APRS report to the packet
			memcpy(beaconPacket.Payload, aprsBuffer, aprsLength);
			beaconPacket.Frame.PayloadLength = aprsLength;

			// Enqueue the packet in the transmit buffer
			xQueueSendToBack(txQueue, &beaconPacket, 0);
		}
	}
}