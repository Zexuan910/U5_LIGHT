#ifndef MOTION_SERVICE_H
#define MOTION_SERVICE_H

#include "mpu6050.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MOTION_SERVICE_STATUS_IDLE = 0,
  MOTION_SERVICE_STATUS_MPU_INIT_FAIL,
  MOTION_SERVICE_STATUS_WAITING_SAMPLE,
  MOTION_SERVICE_STATUS_RUNNING,
  MOTION_SERVICE_STATUS_READ_FAIL
} MotionServiceStatus;

typedef struct {
  MotionServiceStatus status;
  Mpu6050Status mpuStatus;
  uint8_t mpuReady;
  uint8_t mpuIntLevel;
  uint8_t mpuAddress7bit;
  uint8_t mpuWhoAmI;
  uint8_t outputUpdated;
  int16_t accelMg[3];
  int16_t gyroMdps[3];
  float accelG[3];
  float gyroRadS[3];
  uint32_t elapsedMs;
  uint32_t steps;
  uint32_t ropeCount;
  uint32_t distanceCm;
  uint16_t instantSpeedCms;
  uint16_t averageSpeedCms;
  uint16_t strideCm;
  uint16_t cadenceSpm;
  uint16_t activityX100;
  uint32_t pollCount;
  uint32_t sampleCount;
  uint32_t outputCount;
  uint32_t readFailCount;
  uint32_t lastSampleTick;
  uint32_t lastPollTick;
  int32_t lastHalStatus;
  uint32_t i2cError;
} MotionServiceSnapshot;

const MotionServiceSnapshot *MotionService_Init(void);
const MotionServiceSnapshot *MotionService_Poll(uint32_t nowMs);
const MotionServiceSnapshot *MotionService_GetSnapshot(void);
const char *MotionService_StatusName(MotionServiceStatus status);

#ifdef __cplusplus
}
#endif

#endif
