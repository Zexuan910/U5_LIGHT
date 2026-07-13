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

extern volatile uint32_t g_imu_fifo_frame_count;
extern volatile uint32_t g_imu_direct_frame_count;
extern volatile uint32_t g_imu_read_error_count;
extern volatile uint32_t g_imu_recovery_count;
extern volatile uint16_t g_imu_last_fifo_bytes;

void IMU_Sensor_Init(void);
bool IMU_Sensor_Recover(void);
bool IMU_Sensor_IsReady(void);
bool IMU_Sensor_IsCalibrated(void);
bool IMU_Sensor_ResetFifo(void);
uint16_t IMU_Sensor_PendingSamples(void);
bool IMU_Sensor_Read(IMU_SensorSample* sample);

#ifdef __cplusplus
}
#endif

#endif
