#ifndef APRS_H
#define APRS_H
#include <stdint.h>

typedef struct
{
	float Lat;
	float Lon;
	uint8_t SymbolTable;
	uint8_t Symbol;
} AprsPositionT;

typedef struct
{
	AprsPositionT Position;
	uint32_t Timestamp;
	uint8_t* Comment;
	uint32_t CommentLength;
} AprsPositionReportT;

const uint32_t AprsMakePosition(uint8_t* buffer, const AprsPositionReportT* report);


#endif // !APRS_H
