#ifndef AX25_H
#define AX25_H

#include <stdint.h>

#define CALL_SIZE		6

// Ax25 framing struct
typedef struct
{
	uint8_t Source[6];
	uint8_t Destination[6];
	uint8_t* Path;
	uint32_t PathLen;
	uint8_t SourceSsid;
	uint8_t DestinationSsid;
	uint8_t* Payload;
	uint32_t PayloadLength;
	uint8_t PreFlagCount;
	uint8_t PostFlagCount;
} Ax25FrameT;

uint32_t Ax25BuildUnPacket(const Ax25FrameT* frame, uint8_t* outputBuffer);

#endif // !AX25_H
