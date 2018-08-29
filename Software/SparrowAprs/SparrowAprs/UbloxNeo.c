#include <stm32f4xx_hal.h>
#include <string.h>
#include "Nmea0183.h"
#include "Helpers.h"

void UbloxNeoInit(void)
{
}

static uint8_t NmeaInsertChecksum(uint8_t* buffer, const uint32_t length)
{
	uint8_t checksum = 0;
	uint32_t i = 1;

	for (; i < length; i++)
	{
		// If we are at the end of the checksummed area
		if (buffer[i] == '*')
		{
			i++;
			break;
		}

		checksum ^= buffer[i];
	}

	// Check if we have enough space to insert the checksum
	if (i + 2 > length)
	{
		return 0;
	}

	// Insert the checksum (high hibble first)
	buffer[i++] = Int2HexDigit((checksum >> 4) & 0x0f);
	buffer[i++] = Int2HexDigit(checksum & 0x0f);

	return 1;
}

// Set the output rate for a sentence
// UBX-13003221 - R15, page 130
void UbloxNeoSetOutputRate(const char* msgId, const uint8_t rate)
{
	uint8_t configStr[31];

	// Format command
	sprintf((char*)configStr, "$PUBX,40,%.3s,0,%i,0,0,0,0*XX\r\n", msgId, rate);

	// Checksum
	NmeaInsertChecksum(configStr, 31);

	// Send
	HAL_UART_Transmit(Nmea0183GetUartHandle(), configStr, 31, 5000);
}
