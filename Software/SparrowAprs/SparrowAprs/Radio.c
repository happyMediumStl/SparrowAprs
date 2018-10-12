#include <stm32f4xx_hal.h>
#include "FreeRTOS.h"
#include "task.h"
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
#include "Watchdog.h"

// Message output queue
static QueueHandle_t txQueue;
static QueueHandle_t rxQueue;

static TaskHandle_t radioTaskHandle = NULL;

// Audio buffers
#define AUDIO_BUFFER_SIZE		22000
static uint8_t audioOut[AUDIO_BUFFER_SIZE];
static uint8_t audioIn[AUDIO_BUFFER_SIZE];

#define AX25_BUFFER_SIZE	500
static uint8_t ax25Buffer[AX25_BUFFER_SIZE];

#define TX_RX_QUEUE_SIZE	10

void RadioTask(void* pvParameters);
static void Dra818AprsInit(void);

#define PTT_DOWN_DELAY		50
#define PTT_UP_DELAY		20

void RadioInit(void)
{
	// Init queues
	txQueue = xQueueCreate(TX_RX_QUEUE_SIZE, sizeof(RadioPacketT));
	rxQueue = xQueueCreate(TX_RX_QUEUE_SIZE, sizeof(RadioPacketT));
}

QueueHandle_t* RadioGetTxQueue(void)
{
	return txQueue;
}

QueueHandle_t* RadioGetRxQueue(void)
{
	return rxQueue;
}

void RadioTaskStart(void)
{
	// Create task
	xTaskCreate(RadioTask,
		"Radio",
		300,
		NULL,
		7,
		&radioTaskHandle);
}

static void Dra818AprsInit(void)
{
	// Bring up the module
	Dra818IoPowerUp();

	// Wait for the module to init
	vTaskDelay(100 / portTICK_PERIOD_MS);

	// Connect (to init the module?)
	Dra818Connect();
	vTaskDelay(100 / portTICK_PERIOD_MS);

	// Configure group
	Dra818SetGroup(144.390, 144.390, "0000", "0000", 8);
	vTaskDelay(100 / portTICK_PERIOD_MS);

	// Configure filter
	Dra818SetFilter(1, 0, 0);

	// Set RF power
	Dra818IoSetLowRfPower();
}

// Radio manager task
void RadioTask(void* pvParameters)
{
	uint32_t encodedAudioLength;
	uint32_t ax25Len;
	TickType_t lastTaskTime = 0;
	RadioPacketT packetOut;

	// Init DRA radio module
	Dra818AprsInit();

	// Airtime / radio management loop
	while (1)
	{
		// Block until it's time to start
		vTaskDelayUntil(&lastTaskTime, 10);

		// Check if we have a packet to transmit
		if (!xQueueIsQueueEmptyFromISR(txQueue))
		{
			// Get the packet
			if(!xQueueReceive(txQueue, &packetOut, 0))
			{
				continue;
			}

			printf("[TX] %.*s\r\n", (int)packetOut.Frame.PayloadLength, packetOut.Payload);
			
			// We have something to transmit, build and encode the packet
			packetOut.Frame.Path = packetOut.Path;
			packetOut.Frame.Payload = packetOut.Payload;
			ax25Len = Ax25BuildUnPacket(&packetOut.Frame, ax25Buffer);

			// Audio encoding
			encodedAudioLength = AfskHdlcEncode(ax25Buffer,
				ax25Len,
				packetOut.Frame.PreFlagCount,
				ax25Len - packetOut.Frame.PostFlagCount,
				audioOut,
				AUDIO_BUFFER_SIZE
			);

			// If the encoded audio length is zero, the encode output buffer wasn't big enough to accomidate the entire payload
			// It might be mission critical that this gets out in a worst case scenario
			// So just transmit the entire buffer...
			if (!encodedAudioLength)
			{
				encodedAudioLength = AUDIO_BUFFER_SIZE;
			}

			// Start xmit
			LedOn(LED_2);
			Dra818IoPttOn();
			Dra818IoSetHighRfPower();
			
			// Wait to play until after we PTT down
			vTaskDelay(PTT_DOWN_DELAY / portTICK_PERIOD_MS);

			// Play audio
			AudioPlay(audioOut, encodedAudioLength);

			// Block while we're transmitting
			AudioOutWait(portMAX_DELAY);

			// Wait a little longer before we PTT up
			vTaskDelay(PTT_UP_DELAY / portTICK_PERIOD_MS);

			// Stop Xmit
			Dra818IoPttOff();
			Dra818IoSetLowRfPower();
			LedOff(LED_2);
		}
	}
}