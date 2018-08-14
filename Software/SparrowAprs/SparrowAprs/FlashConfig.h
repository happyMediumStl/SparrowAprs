#ifndef FLASHCONFIG_H
#define FLASHCONFIG_H

#include <stdint.h>
#include "Config.h"

void FlashConfigInit(void);
void FlashConfigLoadDefaults(void);
void FlashConfigLoadFromMemory(const ConfigT* configIn);
uint8_t FlashConfigLoad(void);
uint8_t FlashConfigSave(void);
ConfigT* FlashConfigGetPtr(void);

#endif // !FLASHCONFIG_H
