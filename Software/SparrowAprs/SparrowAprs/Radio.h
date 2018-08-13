#ifndef RADIO_H
#define RADIO_H
#include "FreeRTOS.h"
#include <stdint.h>
#include "task.h"
#include "queue.h"
#include "Ax25.h"

typedef struct
{
	Ax25FrameT Frame;
	uint8_t Path[56];
	uint8_t Payload[200];
	TickType_t Expiration;
} RadioPacketT;

void RadioInit(void);
void RadioTaskStart(void);
QueueHandle_t* RadioGetTxQueue(void);
QueueHandle_t* RadioGetRxQueue(void);

#endif // !RADIO_H
