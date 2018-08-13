#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

void AudioInit(void);

// Audio out
void AudioPlay(const uint8_t* buffer, const uint32_t len);
uint8_t AudioIsPlaying(void);
void AudioOutWait(const uint32_t ticks);

// Audio In
void AudioRecord(uint8_t* buffer, const uint32_t length);
uint8_t AudioIsRecording(void);
void AudioInWait(const uint32_t ticks);

void AudioStartRecording(uint8_t* buffer, const uint32_t length);

#endif // !AUDIO_H
