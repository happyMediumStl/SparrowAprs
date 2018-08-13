/*
	Simple Ax25 Packet Generator

*/

#include <stdint.h>
#include <string.h>
#include "Ax25.h"

#define FLAG_BYTE		0x7e

static uint16_t CrcUpdate(const uint16_t crc, uint8_t data);
static uint16_t Crc(const uint8_t* data, const uint32_t length);

void Ax25Init(void)
{
}

uint32_t Ax25BuildUnPacket(const Ax25FrameT* frame, uint8_t* outputBuffer)
{
	uint32_t i = 0;
	uint32_t outputBufferPtr = 0;
	uint16_t crc;

	// Begin building the packet buffer

	// Write preamble to output buffer
	for(i = 0 ; i < frame->PreFlagCount ; i++)
	{
		outputBuffer[outputBufferPtr++] = FLAG_BYTE;
	}

	// Destination callsign
	memcpy(outputBuffer + outputBufferPtr, frame->Destination, CALL_SIZE);
	outputBufferPtr += CALL_SIZE;

	// Destination SSID
	outputBuffer[outputBufferPtr++] = 0b01110000 | (frame->DestinationSsid & 0x0f);

	// Source callsign
	memcpy(outputBuffer + outputBufferPtr, frame->Source, CALL_SIZE);
	outputBufferPtr += CALL_SIZE;

	// Source callsign ssid
	outputBuffer[outputBufferPtr++] = 0b00110000 | (frame->SourceSsid & 0x0f);

	// Path
	memcpy(outputBuffer + outputBufferPtr, frame->Path, frame->PathLen);
	outputBufferPtr +=  frame->PathLen;

	// For each byte in the addresses, we need to left shift by 1
	// The last address byte's LSB is set to indicate end of address bits
	for (i = frame->PreFlagCount; i < outputBufferPtr; i++)
	{
		outputBuffer[i] <<= 1;
	}

	outputBuffer[outputBufferPtr - 1] |= 0x01;

	// Control field
	outputBuffer[outputBufferPtr++] = 0x03;

	// Packet ID
	outputBuffer[outputBufferPtr++] = 0xF0;

	// Payload
	memcpy(outputBuffer + outputBufferPtr, frame->Payload, frame->PayloadLength);
	outputBufferPtr += frame->PayloadLength;

	// Compute the checksum
	crc = Crc(outputBuffer + frame->PreFlagCount, outputBufferPtr - frame->PreFlagCount);
	outputBuffer[outputBufferPtr++] = crc;
	outputBuffer[outputBufferPtr++] = (crc >> 8);

	// Write postamble to output buffer
	for (i = 0; i < frame->PostFlagCount; i++)
	{
		outputBuffer[outputBufferPtr++] = FLAG_BYTE;
	}

	return outputBufferPtr;
}

static uint16_t Crc(const uint8_t* data, const uint32_t length)
{
	uint16_t crc = 0xffff;
	uint8_t temp;
	uint32_t i;

	for (i = 0; i < length; i++)
	{
		temp = *(data++);
		temp ^= crc & 0xff;
		temp ^= temp << 4;
		crc = ((((uint16_t)temp << 8) | (crc >> 8)) ^ (uint8_t)(temp >> 4) ^ ((uint16_t)temp << 3));
	}
	
	return ~crc;
}