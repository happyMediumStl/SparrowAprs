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
} AprsPositionReportT;

const uint32_t AprsMakePosition(uint8_t* buffer, const AprsPositionReportT* report);
const uint32_t AprsMakeExtCourseSpeed(uint8_t* buffer, const uint8_t course, const uint16_t speed);

#endif // !APRS_H
