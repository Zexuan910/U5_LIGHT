#include "battery_monitor.h"

#define BATTERY_ADC_REFERENCE_MV 3300UL
#define BATTERY_ADC_FULL_SCALE 16383UL
#define BATTERY_SAMPLE_COUNT 8U
#define BATTERY_UPDATE_PERIOD_MS 10000UL
#define BATTERY_ADC_TIMEOUT_MS 5UL
#define BATTERY_VALID_MIN_MV 2500UL
#define BATTERY_VALID_MAX_MV 4500UL

typedef struct {
  uint16_t millivolts;
  uint8_t percent;
} BatteryCurvePoint;

static const BatteryCurvePoint battery_curve[] = {
  {3300U, 0U},
  {3500U, 3U},
  {3650U, 7U},
  {3700U, 12U},
  {3750U, 20U},
  {3800U, 30U},
  {3850U, 40U},
  {3900U, 50U},
  {3950U, 60U},
  {4000U, 70U},
  {4050U, 80U},
  {4100U, 90U},
  {4150U, 95U},
  {4200U, 100U}
};

volatile uint32_t g_battery_voltage_mv = 0UL;
volatile uint8_t g_battery_percent = 0U;
volatile uint8_t g_battery_valid = 0U;
volatile uint8_t g_battery_last_error = 0U;
volatile uint32_t g_battery_adc_state = 0UL;
volatile uint32_t g_battery_adc_error = 0UL;
volatile uint32_t g_battery_adc_raw = 0UL;

static ADC_HandleTypeDef* battery_adc = NULL;
static uint32_t battery_last_attempt_ms = 0UL;
static uint8_t battery_has_attempted = 0U;

static uint8_t BatteryMonitor_VoltageToPercent(uint32_t millivolts)
{
  uint32_t i;

  if (millivolts <= battery_curve[0].millivolts)
  {
    return battery_curve[0].percent;
  }

  for (i = 1UL; i < (sizeof(battery_curve) / sizeof(battery_curve[0])); ++i)
  {
    if (millivolts <= battery_curve[i].millivolts)
    {
      uint32_t mv_low = battery_curve[i - 1UL].millivolts;
      uint32_t mv_high = battery_curve[i].millivolts;
      uint32_t pct_low = battery_curve[i - 1UL].percent;
      uint32_t pct_high = battery_curve[i].percent;

      return (uint8_t)(pct_low +
                       (((millivolts - mv_low) * (pct_high - pct_low) +
                         ((mv_high - mv_low) / 2UL)) /
                        (mv_high - mv_low)));
    }
  }

  return 100U;
}

static uint8_t BatteryMonitor_ReadMillivolts(uint32_t* millivolts)
{
  uint32_t raw_sum = 0UL;
  uint32_t i;

  if ((battery_adc == NULL) || (millivolts == NULL))
  {
    return 0U;
  }

  for (i = 0UL; i < BATTERY_SAMPLE_COUNT; ++i)
  {
    if (HAL_ADC_Start(battery_adc) != HAL_OK)
    {
      g_battery_last_error = 1U;
      (void)HAL_ADC_Stop(battery_adc);
      return 0U;
    }
    if (HAL_ADC_PollForConversion(battery_adc, BATTERY_ADC_TIMEOUT_MS) != HAL_OK)
    {
      g_battery_last_error = 2U;
      (void)HAL_ADC_Stop(battery_adc);
      return 0U;
    }

    raw_sum += HAL_ADC_GetValue(battery_adc);
    if (HAL_ADC_Stop(battery_adc) != HAL_OK)
    {
      g_battery_last_error = 3U;
      return 0U;
    }
  }

  raw_sum = (raw_sum + (BATTERY_SAMPLE_COUNT / 2U)) / BATTERY_SAMPLE_COUNT;
  g_battery_adc_raw = raw_sum;
  *millivolts = (raw_sum * BATTERY_ADC_REFERENCE_MV *
                 BATTERY_MONITOR_DIVIDER_NUMERATOR +
                 ((BATTERY_ADC_FULL_SCALE * BATTERY_MONITOR_DIVIDER_DENOMINATOR) / 2UL)) /
                (BATTERY_ADC_FULL_SCALE * BATTERY_MONITOR_DIVIDER_DENOMINATOR);

  if ((*millivolts < BATTERY_VALID_MIN_MV) ||
      (*millivolts > BATTERY_VALID_MAX_MV))
  {
    g_battery_last_error = 6U;
    g_battery_valid = 0U;
    return 0U;
  }

  return 1U;
}

uint8_t BatteryMonitor_Init(ADC_HandleTypeDef* hadc)
{
  battery_adc = hadc;
  battery_has_attempted = 0U;
  g_battery_valid = 0U;

  if (battery_adc == NULL)
  {
    g_battery_last_error = 4U;
    return 0U;
  }

  if (HAL_ADCEx_Calibration_Start(battery_adc, ADC_CALIB_OFFSET,
                                  ADC_SINGLE_ENDED) != HAL_OK)
  {
    g_battery_last_error = 5U;
    g_battery_adc_state = HAL_ADC_GetState(battery_adc);
    g_battery_adc_error = HAL_ADC_GetError(battery_adc);
    return 0U;
  }

  BatteryMonitor_Update(HAL_GetTick());
  return g_battery_valid;
}

void BatteryMonitor_Update(uint32_t now_ms)
{
  uint32_t measured_mv;

  if ((battery_has_attempted != 0U) &&
      ((now_ms - battery_last_attempt_ms) < BATTERY_UPDATE_PERIOD_MS))
  {
    return;
  }

  battery_has_attempted = 1U;
  battery_last_attempt_ms = now_ms;
  if (BatteryMonitor_ReadMillivolts(&measured_mv) == 0U)
  {
    if (battery_adc != NULL)
    {
      g_battery_adc_state = HAL_ADC_GetState(battery_adc);
      g_battery_adc_error = HAL_ADC_GetError(battery_adc);
    }
    return;
  }

  if (g_battery_valid != 0U)
  {
    measured_mv = ((g_battery_voltage_mv * 3UL) + measured_mv + 2UL) / 4UL;
  }

  g_battery_voltage_mv = measured_mv;
  g_battery_percent = BatteryMonitor_VoltageToPercent(measured_mv);
  g_battery_valid = 1U;
  g_battery_last_error = 0U;
  g_battery_adc_state = HAL_ADC_GetState(battery_adc);
  g_battery_adc_error = HAL_ADC_GetError(battery_adc);
}

uint8_t BatteryMonitor_IsValid(void)
{
  return g_battery_valid;
}

uint8_t BatteryMonitor_GetPercent(void)
{
  return g_battery_percent;
}

uint32_t BatteryMonitor_GetMillivolts(void)
{
  return g_battery_voltage_mv;
}
