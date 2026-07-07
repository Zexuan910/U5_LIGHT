#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MPU6050_STATUS_IDLE = 0,
  MPU6050_STATUS_I2C_INIT_FAIL,
  MPU6050_STATUS_NOT_FOUND,
  MPU6050_STATUS_WHOAMI_FAIL,
  MPU6050_STATUS_CONFIG_FAIL,
  MPU6050_STATUS_READY,
  MPU6050_STATUS_READ_FAIL
} Mpu6050Status;

typedef struct {
  Mpu6050Status status;
  uint8_t address7bit;
  uint8_t whoAmI;
  uint8_t intLevel;
  uint8_t sampleReady;
  int16_t rawAccel[3];
  int16_t rawGyro[3];
  int16_t rawTemp;
  int16_t accelMg[3];
  int16_t gyroMdps[3];
  float accelG[3];
  float gyroRadS[3];
  int16_t temperatureX10;
  uint32_t readCount;
  uint32_t readFailCount;
  uint32_t lastReadTick;
  uint32_t i2cError;
  int32_t lastHalStatus;
} Mpu6050Snapshot;

const Mpu6050Snapshot *Mpu6050_Init(void);
const Mpu6050Snapshot *Mpu6050_Read(void);
const Mpu6050Snapshot *Mpu6050_GetSnapshot(void);
const char *Mpu6050_StatusName(Mpu6050Status status);

#ifdef __cplusplus
}
#endif

#endif
