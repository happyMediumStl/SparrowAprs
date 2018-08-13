#include <stm32f4xx_hal.h>
#include <string.h>
#include <stdarg.h>
#include "Audio.h"
#include "Queuex.h"
#include "Helpers.h"
#include "TokenIterate.h"
#include "Nmea0183.h"

/*
	Simple NMEA183 parser
		-- Chris Seto, 2018

	Resources
		* http://www.gpsinformation.org/dale/nmea.htm
*/

// Sentence processors
static void ParseGsa(const uint32_t length);
static void ParseGga(const uint32_t length);
static void ParseRmc(const uint32_t length);
static void ParseZda(const uint32_t length);
static void ProcessSentence(const uint32_t length);

// Packet extractors
static uint8_t ExtractTime(TokenIterateT* t, NmeaTimeT* time);
static uint8_t ExtractDate(TokenIterateT* t, NmeaDateT* date);
static uint8_t ExtractPosition(TokenIterateT* t, NmeaPositionT* pos);
static uint8_t ExtractChar(TokenIterateT* t, uint8_t* c);
static uint8_t ExtractFloat(TokenIterateT* t, float* x);
static uint8_t ExtractInt(TokenIterateT* t, int32_t* x);

typedef struct
{
	char Header[5];
	void(*Processor)(const uint32_t);
} NmeaProcessor;

// Task
static void Nmea0183Task(void * pvParameters);
static TaskHandle_t idleTaskHandle = NULL;

// Serial DMA buffer
#define DMA_BUFFER_SIZE			1000
uint8_t dmaBuffer[DMA_BUFFER_SIZE];
static volatile uint32_t dmaHead = 0;

// Nmea data queue
#define NMEA_BUFFER_SIZE		1000
uint8_t nmeaBuffer[NMEA_BUFFER_SIZE];
static struct QueueT nmeaQueue;

// Message output queue
static QueueHandle_t NmeaMessageQueue;

#define NMEA_PACKET_BUFFER_SIZE	100
static uint8_t packetBuffer[NMEA_PACKET_BUFFER_SIZE];

#define PARSE_STATE_HEADER		0
#define PARSE_STATE_PAYLOAD		1
#define PARSE_STATE_CHECKSUM0	2
#define PARSE_STATE_CHECKSUM1	3
static uint32_t parserState = PARSE_STATE_HEADER;

// NMEA processors
#define NMEA_PROCESSOR_COUNT	4
NmeaProcessor processors[NMEA_PROCESSOR_COUNT] = {
	{ "GPGSA", ParseGsa },
	{ "GPGGA", ParseGga },
	{ "GPRMC", ParseRmc },
	{ "GPZDA", ParseZda }
};

// Peripherl handles
static UART_HandleTypeDef UartHandle;
static DMA_HandleTypeDef hdma_rx;

static void InitUart(void)
{
	// Init Uart
	GPIO_InitTypeDef GPIO_InitStruct;

	// Enable clocks
	__GPIOC_CLK_ENABLE();
	__USART3_CLK_ENABLE();
	__DMA1_CLK_ENABLE();
	
	// Configure GPIO
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	
	// Configure the USART peripheral
	UartHandle.Instance          = USART3;
	UartHandle.Init.BaudRate     = 115200;
	UartHandle.Init.WordLength   = UART_WORDLENGTH_8B;
	UartHandle.Init.StopBits     = UART_STOPBITS_1;
	UartHandle.Init.Parity       = UART_PARITY_NONE;
	UartHandle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
	UartHandle.Init.Mode         = UART_MODE_RX | UART_MODE_TX;

	// Commit the USART
	HAL_UART_Init(&UartHandle);

	// DMA USART3
	// RX: (DMA1, Stream 1, channel 4)
	// TX: (DMA1, Stream 3, Channel 4), (DMA1, Stream 4, Channel 7)
	hdma_rx.Instance                 = DMA1_Stream1;
	hdma_rx.Init.Channel             = DMA_CHANNEL_4;
	hdma_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
	hdma_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
	hdma_rx.Init.MemInc              = DMA_MINC_ENABLE;
	hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	hdma_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
	hdma_rx.Init.Mode                = DMA_CIRCULAR;
	hdma_rx.Init.Priority            = DMA_PRIORITY_HIGH;
	hdma_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
	hdma_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
	hdma_rx.Init.MemBurst            = DMA_MBURST_SINGLE;   // DMA_MBURST_SINGLE DMA_MBURST_INC4
	hdma_rx.Init.PeriphBurst         = DMA_PBURST_SINGLE;

	// Init DMA
	HAL_DMA_Init(&hdma_rx);

	// Link DMA
	__HAL_LINKDMA(&UartHandle, hdmarx, hdma_rx);

	// Start DMA
	HAL_UART_Receive_DMA(&UartHandle, (uint8_t*)&dmaBuffer, DMA_BUFFER_SIZE);
}

uint8_t Nmea0183Init(void)
{
	// Init RTOS queue
	NmeaMessageQueue = xQueueCreate(10, sizeof(GenericNmeaMessageT));

	if (NmeaMessageQueue == NULL)
	{
		return 0;
	}

	// Start serial port
	InitUart();

	return 1;
}

void Nmea0183StartParser(void)
{
	QueueInit(&nmeaQueue, nmeaBuffer, NMEA_BUFFER_SIZE);

	// Start Task
	xTaskCreate(Nmea0183Task,
		"Nmea0183ParserTask",
		256,
		NULL,
		tskIDLE_PRIORITY,
		&idleTaskHandle);
}

// Verify a packet checksum
static uint8_t VerifyChecksum(const uint8_t x, const uint8_t* buffer, const uint32_t length)
{
	uint8_t checksum = 0;
	uint32_t i = 0;

	for (i = 0; i < length; i++)
	{
		checksum ^= buffer[i];
	}

	return (x == checksum);
}

static void CopyFromDma(void)
{
	uint32_t endIndex;

	// Get the index of the end of the buffer
	endIndex =  DMA_BUFFER_SIZE - DMA1_Stream1->NDTR;

	// If there is nothing to copy
	// If you let the buffer loop back, you will lose the entire buffer
	if (endIndex == dmaHead)
	{
		return;
	}

	// Check if the buffer looped over
	if (endIndex < dmaHead)
	{
		// Copy from the end of the dma buffer
		QueueAppendBuffer(&nmeaQueue, dmaBuffer + dmaHead, DMA_BUFFER_SIZE - dmaHead);

		// It has. We need to copy from the end of the circ buffer, then again from the start
		// Copy from the start of the dma buffer
		QueueAppendBuffer(&nmeaQueue, dmaBuffer, endIndex);
	}
	else
	{
		// Copy from the dmaHead to the endindex
		QueueAppendBuffer(&nmeaQueue, dmaBuffer + dmaHead, endIndex - dmaHead);
	}

	// Relocate the DMA head to the end of where we just read
	dmaHead = endIndex;
}

// Parse NMEA
static void Nmea0183Task(void * pvParameters)
{
	uint32_t parsePtr = 0;
	uint8_t frameChecksum;
	uint32_t bufferPtr;
	uint8_t workingByte;
	TickType_t xLastNmeaTime = 0;

	// Parse NMEA
	while (1)
	{
		// Block until it's time to start
		vTaskDelayUntil(&xLastNmeaTime, 10);

		// Import data into the queue from the DMA buffer
		CopyFromDma();

		// While there is data in the queue
		while (parsePtr < QueueCount(&nmeaQueue))
		{
			// Get the current ptr
			workingByte = QueuePeek(&nmeaQueue, parsePtr++);

			// State machine parser
			switch(parserState)
			{
				// Look for and consume header byte $
				case PARSE_STATE_HEADER:
					if (workingByte == '$')
					{
						parserState = PARSE_STATE_PAYLOAD;
					}

					// Dequeue whatever we get at this point, header or not
					QueueDequeqe(&nmeaQueue, 1);
					parsePtr = 0;
					bufferPtr = 0;
					break;

				// Wait until we get past the payload
				case PARSE_STATE_PAYLOAD:
					if (workingByte == '*')
					{
						parserState = PARSE_STATE_CHECKSUM0;
					}
					else
					{
						packetBuffer[bufferPtr++] = workingByte;
					}
					break;

				// First checksum byte
				case PARSE_STATE_CHECKSUM0:

					// Just collect this byte to start building the checksum
					frameChecksum = (Hex2int(workingByte) << 4);

					// Move on
					parserState = PARSE_STATE_CHECKSUM1;
					break;

				// Second checksum byte
				case PARSE_STATE_CHECKSUM1 :
					// Continue building the checksum
					frameChecksum |= Hex2int(workingByte);

					// Attempt to verify the checksum
					if (VerifyChecksum(frameChecksum, packetBuffer, bufferPtr))
					{
						// Dequeue the entire packet if success
						QueueDequeqe(&nmeaQueue, parsePtr);
						parsePtr = 0;

						// Process sentence
						ProcessSentence(bufferPtr);
					}

					// Reset
					parserState = PARSE_STATE_HEADER;
					break;
			}
		}
	}
}

static uint8_t MatchUpTo(const uint8_t* x, const uint8_t* y, const uint32_t len)
{
	uint32_t i;

	for (i = 0; i < len; i++)
	{
		if (x[i] != y[i])
		{
			return 0;
		}
	}

	return 1;
}

static void ProcessSentence(const uint32_t length)
{
	uint32_t i;

	// Try to match to a known processor 
	for (i = 0; i < NMEA_PROCESSOR_COUNT; i++)
	{
		// Check if we have a match
		if (MatchUpTo((uint8_t*)processors[i].Header, packetBuffer, 5))
		{
			// Call the processor
			processors[i].Processor(length);

			return;
		}
	}
}

/*
	Core token types
		char		C
		float		F
		pos			P
		time		T
		date		D
*/
static void NmeaParseTokens(const uint8_t* buffer, const uint32_t length, const char* format, ...)
{
	uint32_t outPtr = 0;
	char* formatPtr = (char*)format;
	uint8_t* token;
	uint32_t tokenLength;
	va_list ap;

	// Create the token iterator
	TokenIterateT t;
	TokenIteratorInit(&t, ',', buffer + 5, length);
	
	// Init var arg list
	va_start(ap, format);

	// For each format descriptor 
	while (*format)
	{
		// Get the current token
		TokenIteratorForward(&t, &token, &tokenLength);

		switch (*format++)
		{
			// Char
			case 'C' :
				ExtractChar(&t, va_arg(ap, uint8_t*));
				break;

			// Float
			case 'F' :
				ExtractFloat(&t, va_arg(ap, float*));
				break;

			// Position
			case 'P' :
				ExtractPosition(&t, va_arg(ap, NmeaPositionT*));
				break;

			// Time
			case 'T':
				ExtractTime(&t, va_arg(ap, NmeaTimeT*));
				break;

			// Date
			case 'D':
				ExtractDate(&t, va_arg(ap, NmeaDateT*));
				break;
		}

		// Advance the descriptor ptr
		formatPtr++;
	}

	va_end(ap);
}

// Extract time
static uint8_t ExtractTime(TokenIterateT* t, NmeaTimeT* time)
{
	uint8_t* token;
	uint32_t tokenLength;

	// Get the token
	TokenIteratorForward(t, &token, &tokenLength);

	// Check length
	if (tokenLength < 6)
	{
		return 0;
	}

	// Hour
	time->Hour = atoil(token + 0, 2);

	// Minute
	time->Minute = atoil(token + 2, 2);

	// Second (might be a float)
	time->Second = atofl(token + 4, tokenLength - 4);

	return 1;
}

// Extract time
static uint8_t ExtractDate(TokenIterateT* t, NmeaDateT* date)
{
	uint8_t* token;
	uint32_t tokenLength;

	// Get the token
	TokenIteratorForward(t, &token, &tokenLength);

	// Check length
	if(tokenLength < 6)
	{
		return 0;
	}

	// Day
	date->Day = atoil(token + 0, 2);

	// Month
	date->Month = atoil(token + 2, 2);

	// Year
	date->Year = atofl(token + 4, tokenLength - 4);

	return 1;
}

// Extract time
static uint8_t ExtractPosition(TokenIterateT* t, NmeaPositionT* pos)
{
	uint8_t* token;
	uint32_t tokenLength;

	int deg = 0;
	float min = 0;
	uint8_t dir = '-';

	// Get the token for lat
	TokenIteratorForward(t, &token, &tokenLength);

	// Check length
	if (tokenLength < 4)
	{
		return 0;
	}

	// Get the degrees
	deg = atoil(token, 2);

	// Get minutes
	min = atofl(token + 2, tokenLength - 2);

	// Get direction
	if (!ExtractChar(t, &dir))
	{
		return 0;
	}

	pos->Lat = (deg + (min / 60.0)) * ((dir == 'N') ? 1 : -1);
	
	// Get the token for lon
	TokenIteratorForward(t, &token, &tokenLength);

	// Check length
	if (tokenLength < 4)
	{
		return 0;
	}

	// Get the degrees
	deg = atoil(token, 3);

	// Get minutes
	min = atofl(token + 2, tokenLength - 3);

	// Get direction
	if (!ExtractChar(t, &dir))
	{
		return 0;
	}

	pos->Lon = (deg + (min / 60.0)) * ((dir == 'E') ? 1 : -1);
	
	return 1;
}

static uint8_t ExtractChar(TokenIterateT* t, uint8_t* c)
{
	uint8_t* token;
	uint32_t tokenLength;

	// Get the token
	TokenIteratorForward(t, &token, &tokenLength);

	// Check length
	if (tokenLength != 1)
	{
		return 0;
	}

	*c = token[0];

	return 1;
}

static uint8_t ExtractFloat(TokenIterateT* t, float* x)
{
	uint8_t* token;
	uint32_t tokenLength;

	// Get the token
	TokenIteratorForward(t, &token, &tokenLength);

	// Check length
	if (tokenLength == 0)
	{
		return 0;
	}

	*x = atofl(token, tokenLength);

	return 1;
}

static uint8_t ExtractInt(TokenIterateT* t, int32_t* x)
{
	uint8_t* token;
	uint32_t tokenLength;

	// Get the token
	TokenIteratorForward(t, &token, &tokenLength);

	// Check length
	if (tokenLength == 0)
	{
		return 0;
	}

	*x = atoil(token, tokenLength);

	return 1;
}

static uint8_t ExtractByteInt(TokenIterateT* t, int8_t* x)
{
	uint8_t* token;
	uint32_t tokenLength;

	// Get the token
	TokenIteratorForward(t, &token, &tokenLength);

	// Check length
	if(tokenLength == 0)
	{
		return 0;
	}

	*x = atoil(token, tokenLength);

	return 1;
}

//// Setence Parsers ////

static void ParseGsa(const uint32_t length)
{
	/*
		$GPGSA,A,3,04,05,,09,12,,,24,,,,,2.5,1.3,2.1*39

		Where:
			 GSA      Satellite status
			 A        Auto selection of 2D or 3D fix (M = manual) 
			 3        3D fix - values include: 1 = no fix
											   2 = 2D fix
											   3 = 3D fix
			 04,05... PRNs of satellites used for fix (space for 12) 
			 2.5      PDOP (dilution of precision) 
			 1.3      Horizontal dilution of precision (HDOP) 
			 2.1      Vertical dilution of precision (VDOP)
			 *39      the checksum data, always begins with *

	*/
}

static void ParseRmc(const uint32_t length)
{
	/*
	$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A

	Where:
		 RMC          Recommended Minimum sentence C
		 123519       Fix taken at 12:35:19 UTC
		 A            Status A=active or V=Void.
		 4807.038,N   Latitude 48 deg 07.038' N
		 01131.000,E  Longitude 11 deg 31.000' E
		 022.4        Speed over the ground in knots
		 084.4        Track angle in degrees True
		 230394       Date - 23rd of March 1994
		 003.1,W      Magnetic Variation
		 *6A          The checksum data, always begins with *

	*/

	GenericNmeaMessageT msg;
	msg.MessageType = NMEA_MESSAGE_TYPE_RMC;
	TokenIterateT t;
	TokenIteratorInit(&t, ',', packetBuffer + 6, length - 6);

	// Time
	ExtractTime(&t, &msg.Rmc.Time);

	// Fix
	ExtractChar(&t, &msg.Rmc.Fix);

	// Position
	ExtractPosition(&t, &msg.Rmc.Position);

	// Speed
	ExtractFloat(&t, &msg.Rmc.Speed);

	// Track
	ExtractFloat(&t, &msg.Rmc.Track);

	// Date
	ExtractDate(&t, &msg.Rmc.Date);

	// Try to queue it
	xQueueSendToBack(NmeaMessageQueue, &msg, 0);
}

static void ParseGga(const uint32_t length)
{
	/*
		$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47

		Where:
			GGA          Global Positioning System Fix Data
			123519       Fix taken at 12:35:19 UTC
			4807.038,N   Latitude 48 deg 07.038' N
			01131.000,E  Longitude 11 deg 31.000' E
			1            Fix quality: 0 = invalid
									1 = GPS fix (SPS)
									2 = DGPS fix
									3 = PPS fix
						4 = Real Time Kinematic
						5 = Float RTK
									6 = estimated (dead reckoning) (2.3 feature)
						7 = Manual input mode
						8 = Simulation mode
			08           Number of satellites being tracked
			0.9          Horizontal dilution of position
			545.4,M      Altitude, Meters, above mean sea level
			46.9,M       Height of geoid (mean sea level) above WGS84
							ellipsoid
			(empty field) time in seconds since last DGPS update
			(empty field) DGPS station ID number
			*47          the checksum data, always begins with *
	*/

	GenericNmeaMessageT msg;
	msg.MessageType = NMEA_MESSAGE_TYPE_GGA;
	TokenIterateT t;
	TokenIteratorInit(&t, ',', packetBuffer + 6, length - 6);

	// Time
	ExtractTime(&t, &msg.Gga.Time);

	// Position
	ExtractPosition(&t, &msg.Gga.Position);

	// Fix
	ExtractChar(&t, &msg.Gga.Fix);

	// Sats tracked
	ExtractInt(&t, &msg.Gga.SatCount);

	// HDOP
	ExtractFloat(&t, &msg.Gga.Altitude);

	// Altitude
	ExtractFloat(&t, &msg.Gga.Altitude);

	// Altitude units
	ExtractChar(&t, &msg.Gga.AltitudeUnits);

	// Try to queue it
	xQueueSendToBack(NmeaMessageQueue, &msg, 0);
}

static void ParseZda(const uint32_t length)
{
	GenericNmeaMessageT msg;
	msg.MessageType = NMEA_MESSAGE_TYPE_ZDA;
	TokenIterateT t;
	TokenIteratorInit(&t, ',', packetBuffer + 6, length - 6);

	// Time
	ExtractTime(&t, &msg.Zda.Time);

	// Day
	ExtractByteInt(&t, (int8_t*)&msg.Zda.Day);

	// Month
	ExtractByteInt(&t, (int8_t*)&msg.Zda.Month);

	// Year
	ExtractByteInt(&t, (int8_t*)&msg.Zda.Year);

	// Try to queue it
	xQueueSendToBack(NmeaMessageQueue, &msg, 0);
}

QueueHandle_t* Nmea0183GetQueue(void)
{
	return NmeaMessageQueue;
}