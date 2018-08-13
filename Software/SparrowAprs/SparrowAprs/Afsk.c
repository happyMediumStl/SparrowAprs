#include <math.h>
#include <stdint.h>

// AFSK settings
#define BITRATE			1200.0
#define MARK_TONE		1200.0
#define SPACE_TONE		2200.0
#define SAMPLE_FREQ		12000.0			// 7000
//#define SAMPLE_FREQ		42000.0
//#define SAMPLE_FREQ		33600.0
//#define SAMPLE_FREQ		24000.0
//#define SAMPLE_FREQ		19200.0
//#define SAMPLE_FREQ		16800.0
//#define SAMPLE_FREQ		9600.0
//#define SAMPLE_FREQ		8400.0
//#define SAMPLE_FREQ		6000.0
//#define SAMPLE_FREQ		4800.0

#define STUFFING_LENGTH		5

#define PI_2						6.2831853071
const float PHASE_DELTA_MARK		= PI_2 * (MARK_TONE / SAMPLE_FREQ);
const float PHASE_DELTA_SPACE		= PI_2 * (SPACE_TONE / SAMPLE_FREQ);
const float TONE_SAMPLE_DURATION	= SAMPLE_FREQ / BITRATE;

void AfskInit()
{
}

// Include one clock duration of a mark or space tone
static void EncodeTone(const uint8_t symbol, uint8_t* buffer, const uint32_t length, float* phase)
{
	uint32_t iTone = 0;

	// Select tone
	float dPhase = symbol ? PHASE_DELTA_MARK : PHASE_DELTA_SPACE;

	// Encode 1bit of this tone
	for (iTone = 0 ; iTone < length ; iTone++)
	{
		// Add sample to buffer
		*(buffer++) = (uint8_t)(127.0 * (sinf(*phase) + 1.0)); 

		// Increment phase advance
		*phase += dPhase;

		if (*phase > PI_2)
		{
			*phase -= PI_2;
		}
	}
}

// Encode AFSK with NZRI and bitstuffing
uint32_t AfskHdlcEncode(const uint8_t* data, const uint32_t len, const uint32_t startStuff, const uint32_t endStuff, uint8_t* afskOut, const uint32_t maxLen)
{
	// Iterators
	uint32_t iByte = 0;
	uint32_t iBit = 0;

	// Bitstuffing
	uint8_t oneCount = 0;

	// Sin modulation
	float phase = 0;
	float dPhase = 0;

	// Data
	uint8_t workingWord = 0;
	uint8_t currentSymbol = 0;

	// Buffer
	uint8_t* bufferEnd = afskOut + maxLen;
	uint32_t sampleSize = 0;

	// For each byte
	for (iByte = 0 ; iByte < len ; iByte++)
	{
		// Get the working word
		workingWord = data[iByte];

		// Encode bits
		for (iBit = 0; iBit < 8; iBit++)
		{
			// NZRI
			if ((workingWord & 0x01) == 0)
			{
				currentSymbol = !currentSymbol;
				oneCount = 0;
			}
			else
			{
				oneCount++;
			}

			// Encode the bit if we have room
			if (sampleSize + TONE_SAMPLE_DURATION > maxLen)
			{
				return 0;
			}

			EncodeTone(currentSymbol, afskOut + sampleSize, TONE_SAMPLE_DURATION, &phase);
			sampleSize += TONE_SAMPLE_DURATION;

			// Stuff the bit if need be
			if (iByte > startStuff && iByte < endStuff)
			{
				if (oneCount >= STUFFING_LENGTH)
				{
					currentSymbol = !currentSymbol;

					// Encode the bit if we have room
					if (sampleSize + TONE_SAMPLE_DURATION > maxLen)
					{
						return 0;
					}

					EncodeTone(currentSymbol, afskOut + sampleSize, TONE_SAMPLE_DURATION, &phase);
					sampleSize += TONE_SAMPLE_DURATION;
					oneCount = 0;
				}
			}
			else
			{
				oneCount = 0;
			}

			// Advance to the next bit
			workingWord >>= 1;
		}
	}

	// Return back the actual size of the encoded buffer
	return sampleSize;
}