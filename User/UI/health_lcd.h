#ifndef HEALTH_LCD_H
#define HEALTH_LCD_H

#include "health_ui.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void HealthLcd_Init(void);
void HealthLcd_Update(const HealthUiSnapshot *snapshot, uint32_t nowMs);

#ifdef __cplusplus
}
#endif

#endif
