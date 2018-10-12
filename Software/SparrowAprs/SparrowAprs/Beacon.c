#include <stm32f4xx_hal.h>
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "Ax25.h"
#include "Aprs.h"
#include "Beacon.h"
#include "Led.h"
#include "Rtc.h"
#include "time.h"
#include "Radio.h"
#include "Config.h"
#include "FlashConfig.h"
#include "Bme280Shim.h"
#include "Bsp.h"
#include "GpsHub.h"

// APRS packet buffer
#define PACKET_BUFFER_SIZE		500
static uint8_t aprsBuffer[PACKET_BUFFER_SIZE];

void BeaconTask(void* pvParameters);
static TaskHandle_t beaconTaskHandle = NULL;

#define FIRST_BEACON_OFFSET		500

#define PREFLAG_COUNT		10
#define POSTFLAG_COUNT		25

void BeaconInit(void)
{
}

void BeaconStartTask(void)
{
	// Create task
	xTaskCreate(BeaconTask,
		"Beacon",
		450,
		NULL,
		3,
		&beaconTaskHandle);
}

#define METERS_TO_FEET	3.28084f
static float Meters2Feet(const float m)
{
	return m * METERS_TO_FEET;
}

static uint32_t CalculateSmartBeacon(const uint32_t minTime,
	const uint32_t maxTime,
	const float speed,
	const float dSpeed,
	const float dHeading,
	const float cS,
	const float cDs,
	const float cDh)
{
	uint32_t margin = maxTime - minTime;
	uint32_t score = 0;
	uint32_t weightSpeed;
	uint32_t weightdSpeed;
	uint32_t weightdHeading;

	// Calculate weights
	score += cDs * dSpeed;
	score += cS * speed;
	score += cDh * dHeading;

	if (score < maxTime)
	{
		margin -= score;
	}
	else
	{
		margin = 0;
	}

	return minTime + margin;
}

void BeaconTask(void* pvParameters)
{
	QueueHandle_t* txQueue;
	TickType_t lastTaskTime = 0;
	uint32_t aprsLength;
	AprsPositionReportT aprsReport;
	RadioPacketT beaconPacket;
	uint32_t beaconPeriod;
	float temperature;
	float pressure;
	float humidity;
	SituationInfoT situation;

	// Get config
	ConfigT* config = FlashConfigGetPtr();

	// Get the queues
	txQueue = RadioGetTxQueue();

	// If we didn't get any queues, something is really wrong
	if (txQueue == NULL)
	{
		return;
	}

	// Beacon quickly on the first start
	beaconPeriod = FIRST_BEACON_OFFSET;

	// Beacon loop
	while (1)
	{
		// Block until it's time to start
		vTaskDelayUntil(&lastTaskTime, beaconPeriod);

		// Get the latest situation
		GpsHubGetSituation(&situation);

		// Get the current environmental data
		Bme280ShimGetTph(&temperature, &pressure, &humidity);

		// Compute smart beacon period
		if (config->Aprs.UseSmartBeacon)
		{
			beaconPeriod = CalculateSmartBeacon(config->Aprs.BeaconPeriod,
				config->Aprs.SmartBeaconMinimumBeaconPeriod,
				situation.Speed, situation.dSpeed,
				situation.dTrack,
				config->Aprs.SmartBeaconWeightSpeed,
				config->Aprs.SmartBeaconWeightdSpeed,
				config->Aprs.SmartBeaconWeightdHeading
			);
		}
		else
		{
			beaconPeriod = config->Aprs.BeaconPeriod;
		}
	
		// Configure Ax25 frame
		memcpy(beaconPacket.Frame.Source, config->Aprs.Callsign, 6);
		beaconPacket.Frame.SourceSsid = config->Aprs.Ssid;
		memcpy(beaconPacket.Frame.Destination, "APRS  ", 6);
		beaconPacket.Frame.DestinationSsid = 0;
		memcpy(beaconPacket.Path, config->Aprs.Path, 7);
		beaconPacket.Frame.PathLen = 7;

		beaconPacket.Frame.PreFlagCount = PREFLAG_COUNT;
		beaconPacket.Frame.PostFlagCount = POSTFLAG_COUNT;

		// Fill APRS report
		aprsReport.Timestamp = RtcGet();
		aprsReport.Position.Lat = situation.Lat;
		aprsReport.Position.Lon = situation.Lon;
		aprsReport.Position.Symbol = config->Aprs.Symbol;
		aprsReport.Position.SymbolTable = config->Aprs.SymbolTable;

		// Build APRS report
		aprsLength = 0;
		aprsLength += AprsMakePosition(aprsBuffer, &aprsReport);
		aprsLength += AprsMakeExtCourseSpeed(aprsBuffer + aprsLength, (uint8_t)situation.Track, (uint16_t)situation.Speed);

		// Append comment for GPS altitude
		sprintf((char*)aprsBuffer + aprsLength, "A=%06i", (int)Meters2Feet(situation.Altitude));
		aprsLength += 8;

		// Add telemetry comment
		sprintf((char*)aprsBuffer + aprsLength, "/Pa=%06i/Rh=%02.02f/Ti=%02.02f/V=%02.02f", (int)pressure, humidity, temperature, BspGetVSense());
		aprsLength += 35;

		// Add the APRS report to the packet
		memcpy(beaconPacket.Payload, aprsBuffer, aprsLength);
		beaconPacket.Frame.PayloadLength = aprsLength;

		// Enqueue the packet in the transmit buffer
		xQueueSendToBack(txQueue, &beaconPacket, 0);
	}
}