/*
	APRS Packet Encoder
		-- Chris Seto, 2018

	References:
		* http://www.aprs.org/doc/APRS101.PDF

	Example from tracksoar: "/000000h0000.00N/00000.00EO000/000/A=000000/11487/44.37/29.42"

	/000000h0000.00N/00000.00EO000/000/A=000000/11487/44.37/29.42
	/191920h3850.86N/09016.92W-Test123

*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "Aprs.h"

// APRS101, Page 34
// 19 bytes
static void AprsMakePositionCoordinates(uint8_t* buffer, const AprsPositionT* position)
{
	// Get elements of lat
	float latAbs = fabs(position->Lat);
	float latWhole = floorf(latAbs);
	float latMinutes = (latAbs - latWhole) * 60.0;
	float latSeconds = (latMinutes - floorf(latMinutes)) * 100.0;
	uint8_t latChar = (position->Lat > 0) ? 'N' : 'S';

	// Get elements of lon
	float lonAbs = fabs(position->Lon);
	float lonWhole = floorf(lonAbs);
	float lonMinutes = (lonAbs - lonWhole) * 60.0;
	float lonSeconds = (lonMinutes - floorf(lonMinutes)) * 100.0;
	uint8_t lonChar = (position->Lon > 0) ? 'E' : 'W';

	// Format the buffer
	sprintf((char*)buffer, "%02lu%02lu.%02lu%c%c%03lu%02lu.%02lu%c%c", 
		(uint32_t)latWhole,
		(uint32_t)latMinutes,
		(uint32_t)latSeconds,
		latChar,
		position->SymbolTable,
		(uint32_t)lonWhole,
		(uint32_t)lonMinutes,
		(uint32_t)lonSeconds,
		lonChar,
		position->Symbol);
}

// APRS101, Page 32
// 7 bytes
static void AprsMakeTimeHms(uint8_t* buffer, const uint32_t timestamp)
{
	struct tm* timeStruct;

	// Extract timestamp
	timeStruct = gmtime((long*)&timestamp);

	// Check if we got a valid time back
	if (timeStruct == NULL)
	{
		sprintf((char*)buffer, "000000h");
		return;
	}

	// Format buffer
	sprintf((char*)buffer, "%02d%02d%02dh", timeStruct->tm_hour, timeStruct->tm_min, timeStruct->tm_sec);
}

// APRS101, Page 32
const uint32_t AprsMakePosition(uint8_t* buffer, const AprsPositionReportT* report)
{
	uint32_t i;
	uint32_t bufferPtr = 0;

	// Position report, with timestamp, no messaging
	buffer[bufferPtr++] = '/';

	// Encode timestamp
	if (!report->Timestamp)
	{
		sprintf((char*)buffer + bufferPtr, "000000h");
	}
	else
	{
		AprsMakeTimeHms(buffer + bufferPtr, report->Timestamp);
	}
	bufferPtr += 7;

	// Encode position
	// 19 Bytes
	AprsMakePositionCoordinates(buffer + bufferPtr, &report->Position);
	bufferPtr += 19;

	return bufferPtr;
}

const uint32_t AprsMakeExtCourseSpeed(uint8_t* buffer, const uint8_t course, const uint16_t speed)
{
	sprintf((char*)buffer, "%03d/%03d", course, speed);
	return 7;
}