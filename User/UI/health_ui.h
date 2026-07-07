#ifndef HEALTH_UI_H
#define HEALTH_UI_H

#include "gh3018_hrspo2.h"
#include "motion_service.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  HEALTH_UI_MODE_WALK = 0,
  HEALTH_UI_MODE_RUN,
  HEALTH_UI_MODE_ROPE,
  HEALTH_UI_MODE_COUNT
} HealthUiMode;

typedef struct {
  Gh3018HrSpo2Status ghStatus;
  MotionServiceStatus motionStatus;
  Mpu6050Status mpuStatus;
  uint8_t ghReady;
  uint8_t ghError;
  uint8_t mpuReady;
  uint8_t mpuError;
  uint8_t heartRateValid;
  uint8_t spo2Valid;
  uint8_t heartRate;
  uint8_t heartRateConfidence;
  uint8_t spo2;
  uint8_t spo2Confidence;
  uint8_t wearingState;
  uint32_t steps;
  uint32_t ropeCount;
  uint32_t distanceCm;
  uint16_t instantSpeedCms;
  uint16_t averageSpeedCms;
  uint16_t strideCm;
  uint16_t cadenceSpm;
  uint16_t activityX100;
  int16_t accelMg[3];
  int16_t gyroMdps[3];
  uint32_t ghRefreshCount;
  uint32_t motionOutputCount;
  uint32_t motionReadFailCount;
} HealthUiSnapshot;

void HealthUi_BuildSnapshot(HealthUiSnapshot *snapshot);
const char *HealthUi_ModeName(HealthUiMode mode);

#ifdef __cplusplus
}
#endif

#endif
