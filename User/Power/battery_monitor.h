#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>
#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t valid;
  uint32_t rawAdc;
  uint32_t adcMillivolts;
  uint32_t batteryMillivolts;
  uint8_t percent;
  uint32_t sampleCount;
  uint32_t errorCount;
  HAL_StatusTypeDef lastStatus;
} BatteryMonitorSnapshot;

void BatteryMonitor_Init(ADC_HandleTypeDef *hadc);
const BatteryMonitorSnapshot *BatteryMonitor_Update(uint32_t now_ms);
const BatteryMonitorSnapshot *BatteryMonitor_GetSnapshot(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_MONITOR_H */
