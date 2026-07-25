#ifndef TOUCH_CONTROLLER_H
#define TOUCH_CONTROLLER_H

#include "DEV_Config.h"
#include "main.h"

typedef struct {
  UBYTE pressed;
  UWORD x;
  UWORD y;
} TouchControllerSample;

void TouchController_Init(void);
void TouchController_NotifyInterrupt(void);
TouchControllerSample TouchController_Sample(uint32_t now_ms);

#endif
