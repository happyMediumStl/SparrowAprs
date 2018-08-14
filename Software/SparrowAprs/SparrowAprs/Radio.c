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

// Message output queue
static QueueHandle_t txQueue;
static QueueHandle_t rxQueue;

void RadioTask(void* pvParameters);
static TaskHandle_t radioTaskHandle = NULL;

// Audio buffers
#define AUDIO_BUFFER_SIZE		18000
static uint8_t audioOut[AUDIO_BUFFER_SIZE];
static uint8_t audioIn[AUDIO_BUFFER_SIZE];

#define AX25_BUFFER_SIZE	500
static uint8_t ax25Buffer[AX25_BUFFER_SIZE];

void RadioInit(void)
{
	// Take DRA818 out of powerdown
	//Dra818IoPowerUp();

	//vTaskDelay(50 / portTICK_PERIOD_MS);

	// Configure the radio
	//Dra818SetGroup(0, 144.390, 144.390, "0000", "0000", 4);

	//vTaskDelay(50 / portTICK_PERIOD_MS);

	// Set RF power
	//Dra818IoSetLowRfPower();

	// Init queues
	txQueue = xQueueCreate(10, sizeof(RadioPacketT));
	rxQueue = xQueueCreate(10, sizeof(RadioPacketT));
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
		512,
		NULL,
		3,
		&radioTaskHandle);
}

// Radio manager task
void RadioTask(void* pvParameters)
{
	uint32_t encodedAudioLength;
	uint32_t ax25Len;
	TickType_t lastTaskTime = 0;
	RadioPacketT packetOut;

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
			Dra818IoPttOn();
			LedOn(LED_2);

			// Wait to play until after we PTT down
			vTaskDelay(100 / portTICK_PERIOD_MS);

			// Play audio
			AudioPlay(audioOut, encodedAudioLength);

			// Block while we're transmitting
			AudioOutWait(portMAX_DELAY);

			// Wait a little longer before we PTT up
			vTaskDelay(50 / portTICK_PERIOD_MS);

			// Stop Xmit
			Dra818IoPttOff();
			LedOff(LED_2);
		}
	}
}