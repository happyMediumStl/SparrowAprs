#ifndef LED_H
#define LED_H

enum LED
{
	LED_1,
	LED_2,
	LED_3
};

void LedInit(void);
void LedOff(const enum LED led);
void LedOn(const enum LED led);
void LedToggle(const enum LED led);

#endif // !LED_H
