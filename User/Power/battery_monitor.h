#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>

#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PC2/ADC1_IN3 is fed by the board's battery divider. */
#ifndef BATTERY_MONITOR_DIVIDER_NUMERATOR
#define BATTERY_MONITOR_DIVIDER_NUMERATOR 6U
#endif

#ifndef BATTERY_MONITOR_DIVIDER_DENOMINATOR
#define BATTERY_MONITOR_DIVIDER_DENOMINATOR 1U
#endif

uint8_t BatteryMonitor_Init(ADC_HandleTypeDef* hadc);
void BatteryMonitor_Update(uint32_t now_ms);
uint8_t BatteryMonitor_IsValid(void);
uint8_t BatteryMonitor_GetPercent(void);
uint32_t BatteryMonitor_GetMillivolts(void);

extern volatile uint32_t g_battery_voltage_mv;
extern volatile uint8_t g_battery_percent;
extern volatile uint8_t g_battery_valid;
extern volatile uint8_t g_battery_last_error;
extern volatile uint32_t g_battery_adc_state;
extern volatile uint32_t g_battery_adc_error;
extern volatile uint32_t g_battery_adc_raw;

#ifdef __cplusplus
}
#endif

#endif
