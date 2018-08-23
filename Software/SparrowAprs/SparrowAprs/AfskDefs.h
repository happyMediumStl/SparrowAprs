#ifndef AFSKDEFS_H
#define AFSKDEFS_H
/*
	AFSK settings and constants 

	The following sample rates are known good:
		//#define SAMPLE_FREQ		42000.0
		//#define SAMPLE_FREQ		33600.0
		//#define SAMPLE_FREQ		24000.0
		//#define SAMPLE_FREQ		19200.0
		//#define SAMPLE_FREQ		16800.0
		//#define SAMPLE_FREQ		9600.0
		//#define SAMPLE_FREQ		8400.0
		//#define SAMPLE_FREQ		6000.0
		//#define SAMPLE_FREQ		4800.0
*/

// AFSK settings
#define BITRATE			1200.0
#define MARK_TONE		1200.0
#define SPACE_TONE		2200.0
#define SAMPLE_FREQ		12000.0

// Bit stuffing 
#define STUFFING_LENGTH		5

// Do not edit below

#define PI_2					6.2831853071f
#define PHASE_DELTA_MARK		PI_2 * (MARK_TONE / SAMPLE_FREQ)
#define PHASE_DELTA_SPACE		PI_2 * (SPACE_TONE / SAMPLE_FREQ)
#define TONE_SAMPLE_DURATION	SAMPLE_FREQ / BITRATE

#endif // !AFSKDEFS_H
