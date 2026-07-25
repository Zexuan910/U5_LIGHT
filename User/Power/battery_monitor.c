#include "battery_monitor.h"

#define BATTERY_MONITOR_UPDATE_INTERVAL_MS 1000UL
#define BATTERY_MONITOR_SAMPLE_COUNT 8UL
#define BATTERY_MONITOR_ADC_REF_MV 3300UL
#define BATTERY_MONITOR_ADC_MAX 4095UL
#define BATTERY_MONITOR_DIVIDER_TOP_KOHM 2200UL
#define BATTERY_MONITOR_DIVIDER_BOTTOM_KOHM 1000UL
#define BATTERY_MONITOR_EMPTY_MV 3300UL
#define BATTERY_MONITOR_FULL_MV 4200UL
#define BATTERY_MONITOR_ADC_TIMEOUT_MS 10UL

static ADC_HandleTypeDef *s_hadc = NULL;
static BatteryMonitorSnapshot s_snapshot = {0};
static uint32_t s_last_update_ms = 0UL;

static uint32_t BatteryMonitor_RawToAdcMillivolts(uint32_t raw_adc)
{
  return (uint32_t)((((uint64_t)raw_adc * BATTERY_MONITOR_ADC_REF_MV) +
                     (BATTERY_MONITOR_ADC_MAX / 2UL)) /
                    BATTERY_MONITOR_ADC_MAX);
}

static uint32_t BatteryMonitor_AdcToBatteryMillivolts(uint32_t adc_mv)
{
  return (uint32_t)((((uint64_t)adc_mv *
                      (BATTERY_MONITOR_DIVIDER_TOP_KOHM + BATTERY_MONITOR_DIVIDER_BOTTOM_KOHM)) +
                     (BATTERY_MONITOR_DIVIDER_BOTTOM_KOHM / 2UL)) /
                    BATTERY_MONITOR_DIVIDER_BOTTOM_KOHM);
}

static uint8_t BatteryMonitor_VoltageToPercent(uint32_t battery_mv)
{
  if (battery_mv <= BATTERY_MONITOR_EMPTY_MV)
  {
    return 0U;
  }
  if (battery_mv >= BATTERY_MONITOR_FULL_MV)
  {
    return 100U;
  }

  return (uint8_t)((((battery_mv - BATTERY_MONITOR_EMPTY_MV) * 100UL) +
                    ((BATTERY_MONITOR_FULL_MV - BATTERY_MONITOR_EMPTY_MV) / 2UL)) /
                   (BATTERY_MONITOR_FULL_MV - BATTERY_MONITOR_EMPTY_MV));
}

void BatteryMonitor_Init(ADC_HandleTypeDef *hadc)
{
  s_hadc = hadc;
  s_snapshot.valid = 0U;
  s_snapshot.rawAdc = 0UL;
  s_snapshot.adcMillivolts = 0UL;
  s_snapshot.batteryMillivolts = 0UL;
  s_snapshot.percent = 0U;
  s_snapshot.sampleCount = 0UL;
  s_snapshot.errorCount = 0UL;
  s_snapshot.lastStatus = HAL_OK;
  s_last_update_ms = 0UL;
}

const BatteryMonitorSnapshot *BatteryMonitor_Update(uint32_t now_ms)
{
  uint32_t sum = 0UL;
  uint32_t count = 0UL;
  HAL_StatusTypeDef status = HAL_OK;

  if (s_hadc == NULL)
  {
    s_snapshot.valid = 0U;
    s_snapshot.lastStatus = HAL_ERROR;
    return &s_snapshot;
  }

  if ((s_snapshot.sampleCount != 0UL) &&
      ((now_ms - s_last_update_ms) < BATTERY_MONITOR_UPDATE_INTERVAL_MS))
  {
    return &s_snapshot;
  }

  for (uint32_t i = 0UL; i < BATTERY_MONITOR_SAMPLE_COUNT; i++)
  {
    status = HAL_ADC_Start(s_hadc);
    if (status == HAL_OK)
    {
      status = HAL_ADC_PollForConversion(s_hadc, BATTERY_MONITOR_ADC_TIMEOUT_MS);
    }

    if (status == HAL_OK)
    {
      sum += HAL_ADC_GetValue(s_hadc);
      count++;
    }
    else
    {
      s_snapshot.errorCount++;
    }

    (void)HAL_ADC_Stop(s_hadc);

    if (status != HAL_OK)
    {
      break;
    }
  }

  s_snapshot.lastStatus = status;
  s_last_update_ms = now_ms;

  if (count == 0UL)
  {
    s_snapshot.valid = 0U;
    return &s_snapshot;
  }

  s_snapshot.rawAdc = (sum + (count / 2UL)) / count;
  s_snapshot.adcMillivolts = BatteryMonitor_RawToAdcMillivolts(s_snapshot.rawAdc);
  s_snapshot.batteryMillivolts = BatteryMonitor_AdcToBatteryMillivolts(s_snapshot.adcMillivolts);
  s_snapshot.percent = BatteryMonitor_VoltageToPercent(s_snapshot.batteryMillivolts);
  s_snapshot.sampleCount++;
  s_snapshot.valid = 1U;

  return &s_snapshot;
}

const BatteryMonitorSnapshot *BatteryMonitor_GetSnapshot(void)
{
  return &s_snapshot;
}
