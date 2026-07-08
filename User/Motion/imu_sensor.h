#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  int16_t raw_accel[3];
  int16_t raw_gyro[3];
  float accel_g[3];
  float gyro_rad_s[3];
} IMU_SensorSample;

void IMU_Sensor_Init(void);
bool IMU_Sensor_IsReady(void);
bool IMU_Sensor_ResetFifo(void);
uint16_t IMU_Sensor_PendingSamples(void);
bool IMU_Sensor_Read(IMU_SensorSample* sample);

#ifdef __cplusplus
}
#endif

#endif
