#include "imu_sensor.h"

#include "main.h"

#include <math.h>
#include <stddef.h>

extern I2C_HandleTypeDef hi2c2;

#define MPU6050_ADDR_PRIMARY_7BIT    0x68U
#define MPU6050_ADDR_SECONDARY_7BIT  0x69U
#define MPU6050_REG_SMPLRT_DIV       0x19U
#define MPU6050_REG_CONFIG           0x1AU
#define MPU6050_REG_GYRO_CONFIG      0x1BU
#define MPU6050_REG_ACCEL_CONFIG     0x1CU
#define MPU6050_REG_FIFO_EN          0x23U
#define MPU6050_REG_ACCEL_XOUT_H     0x3BU
#define MPU6050_REG_USER_CTRL        0x6AU
#define MPU6050_REG_PWR_MGMT_1       0x6BU
#define MPU6050_REG_FIFO_COUNT_H     0x72U
#define MPU6050_REG_FIFO_R_W         0x74U
#define MPU6050_REG_WHO_AM_I         0x75U
#define MPU6050_FIFO_FRAME_SIZE      12U
#define MPU6050_OUTPUT_FRAME_SIZE    14U
#define MPU6050_FIFO_CAPACITY        1024U
#define MPU6050_I2C_TIMEOUT_MS       20U
#define MPU6050_CALIBRATION_SAMPLES  64U
#define MPU6050_CALIBRATION_DELAY_MS 20U
#define MPU6050_ACCEL_LSB_PER_G      16384.0f
#define MPU6050_GYRO_LSB_PER_DPS     65.5f
#define MPU6050_DEG_TO_RAD           0.0174532925f

static uint8_t imu_ready = 0U;
static uint8_t imu_calibrated = 0U;
static uint16_t imu_addr = (MPU6050_ADDR_PRIMARY_7BIT << 1);
static float gyro_bias_raw[3] = {0.0f, 0.0f, 0.0f};
static float accel_scale = 1.0f;

volatile uint32_t g_imu_fifo_frame_count = 0U;
volatile uint32_t g_imu_direct_frame_count = 0U;
volatile uint32_t g_imu_read_error_count = 0U;
volatile uint32_t g_imu_recovery_count = 0U;
volatile uint16_t g_imu_last_fifo_bytes = 0U;

static int16_t bytes_to_i16(uint8_t high_byte, uint8_t low_byte)
{
  return (int16_t)(((uint16_t)high_byte << 8) | low_byte);
}

static HAL_StatusTypeDef mpu_write_reg(uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c2,
                           imu_addr,
                           reg,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1U,
                           MPU6050_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef mpu_read_reg(uint8_t reg, uint8_t* value)
{
  return HAL_I2C_Mem_Read(&hi2c2,
                          imu_addr,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          value,
                          1U,
                          MPU6050_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef mpu_read_bytes(uint8_t reg, uint8_t* data, uint16_t size)
{
  return HAL_I2C_Mem_Read(&hi2c2,
                          imu_addr,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          size,
                          MPU6050_I2C_TIMEOUT_MS);
}

static bool mpu_select_address(void)
{
  uint8_t who = 0U;
  const uint16_t addresses[2] = {
    (MPU6050_ADDR_PRIMARY_7BIT << 1),
    (MPU6050_ADDR_SECONDARY_7BIT << 1)
  };

  for (uint8_t i = 0U; i < 2U; i++)
  {
    imu_addr = addresses[i];
    if (mpu_read_reg(MPU6050_REG_WHO_AM_I, &who) == HAL_OK)
    {
      return true;
    }
  }

  return false;
}

static void mpu_calibrate_stationary(void)
{
  int32_t accel_sum[3] = {0, 0, 0};
  int32_t gyro_sum[3] = {0, 0, 0};
  int16_t accel_min[3] = {32767, 32767, 32767};
  int16_t accel_max[3] = {-32768, -32768, -32768};
  int16_t gyro_min[3] = {32767, 32767, 32767};
  int16_t gyro_max[3] = {-32768, -32768, -32768};
  uint8_t frame[MPU6050_OUTPUT_FRAME_SIZE];
  uint16_t valid_samples = 0U;

  imu_calibrated = 0U;
  accel_scale = 1.0f;
  for (uint8_t axis = 0U; axis < 3U; axis++)
  {
    gyro_bias_raw[axis] = 0.0f;
  }

  HAL_Delay(100U);
  for (uint16_t sample_index = 0U; sample_index < MPU6050_CALIBRATION_SAMPLES; sample_index++)
  {
    int16_t accel_raw[3];
    int16_t gyro_raw[3];

    if (mpu_read_bytes(MPU6050_REG_ACCEL_XOUT_H, frame, sizeof(frame)) == HAL_OK)
    {
      accel_raw[0] = bytes_to_i16(frame[0], frame[1]);
      accel_raw[1] = bytes_to_i16(frame[2], frame[3]);
      accel_raw[2] = bytes_to_i16(frame[4], frame[5]);
      gyro_raw[0] = bytes_to_i16(frame[8], frame[9]);
      gyro_raw[1] = bytes_to_i16(frame[10], frame[11]);
      gyro_raw[2] = bytes_to_i16(frame[12], frame[13]);

      for (uint8_t axis = 0U; axis < 3U; axis++)
      {
        accel_sum[axis] += accel_raw[axis];
        gyro_sum[axis] += gyro_raw[axis];
        if (accel_raw[axis] < accel_min[axis])
        {
          accel_min[axis] = accel_raw[axis];
        }
        if (accel_raw[axis] > accel_max[axis])
        {
          accel_max[axis] = accel_raw[axis];
        }
        if (gyro_raw[axis] < gyro_min[axis])
        {
          gyro_min[axis] = gyro_raw[axis];
        }
        if (gyro_raw[axis] > gyro_max[axis])
        {
          gyro_max[axis] = gyro_raw[axis];
        }
      }
      valid_samples++;
    }
    HAL_Delay(MPU6050_CALIBRATION_DELAY_MS);
  }

  if (valid_samples >= (MPU6050_CALIBRATION_SAMPLES * 3U / 4U))
  {
    float accel_average[3];
    float accel_norm;
    uint8_t stationary = 1U;

    for (uint8_t axis = 0U; axis < 3U; axis++)
    {
      accel_average[axis] = (float)accel_sum[axis] / (float)valid_samples;
      if (((int32_t)accel_max[axis] - (int32_t)accel_min[axis] > 2600) ||
          ((int32_t)gyro_max[axis] - (int32_t)gyro_min[axis] > 900) ||
          (fabsf((float)gyro_sum[axis] / (float)valid_samples) > 500.0f))
      {
        stationary = 0U;
      }
    }

    accel_norm = sqrtf(accel_average[0] * accel_average[0] +
                       accel_average[1] * accel_average[1] +
                       accel_average[2] * accel_average[2]);
    if ((accel_norm < (MPU6050_ACCEL_LSB_PER_G * 0.75f)) ||
        (accel_norm > (MPU6050_ACCEL_LSB_PER_G * 1.25f)))
    {
      stationary = 0U;
    }

    if (stationary != 0U)
    {
      for (uint8_t axis = 0U; axis < 3U; axis++)
      {
        gyro_bias_raw[axis] = (float)gyro_sum[axis] / (float)valid_samples;
      }
      accel_scale = MPU6050_ACCEL_LSB_PER_G / accel_norm;
      imu_calibrated = 1U;
    }
  }
}

void IMU_Sensor_Init(void)
{
  imu_ready = 0U;

  if (!mpu_select_address())
  {
    return;
  }

  if (mpu_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00U) != HAL_OK)
  {
    return;
  }
  HAL_Delay(30U);

  (void)mpu_write_reg(MPU6050_REG_CONFIG, 0x03U);
  (void)mpu_write_reg(MPU6050_REG_SMPLRT_DIV, 0x13U);
  (void)mpu_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00U);
  (void)mpu_write_reg(MPU6050_REG_GYRO_CONFIG, 0x08U);
  mpu_calibrate_stationary();
  (void)mpu_write_reg(MPU6050_REG_FIFO_EN, 0x78U);
  (void)mpu_write_reg(MPU6050_REG_USER_CTRL, 0x44U);

  imu_ready = 1U;
}

bool IMU_Sensor_Recover(void)
{
  imu_ready = 0U;
  g_imu_recovery_count++;
  if (!mpu_select_address())
  {
    return false;
  }

  if (mpu_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00U) != HAL_OK)
  {
    return false;
  }
  HAL_Delay(10U);
  if ((mpu_write_reg(MPU6050_REG_CONFIG, 0x03U) != HAL_OK) ||
      (mpu_write_reg(MPU6050_REG_SMPLRT_DIV, 0x13U) != HAL_OK) ||
      (mpu_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00U) != HAL_OK) ||
      (mpu_write_reg(MPU6050_REG_GYRO_CONFIG, 0x08U) != HAL_OK) ||
      (mpu_write_reg(MPU6050_REG_FIFO_EN, 0x78U) != HAL_OK) ||
      (mpu_write_reg(MPU6050_REG_USER_CTRL, 0x44U) != HAL_OK))
  {
    return false;
  }

  imu_ready = 1U;
  return true;
}

bool IMU_Sensor_IsReady(void)
{
  return imu_ready != 0U;
}

bool IMU_Sensor_IsCalibrated(void)
{
  return imu_calibrated != 0U;
}

bool IMU_Sensor_ResetFifo(void)
{
  if (imu_ready == 0U)
  {
    return false;
  }

  if (mpu_write_reg(MPU6050_REG_FIFO_EN, 0x00U) != HAL_OK)
  {
    imu_ready = 0U;
    return false;
  }
  if (mpu_write_reg(MPU6050_REG_USER_CTRL, 0x44U) != HAL_OK)
  {
    imu_ready = 0U;
    return false;
  }
  if (mpu_write_reg(MPU6050_REG_FIFO_EN, 0x78U) != HAL_OK)
  {
    imu_ready = 0U;
    return false;
  }

  return true;
}

uint16_t IMU_Sensor_PendingSamples(void)
{
  uint8_t count_bytes[2];
  uint16_t fifo_bytes;

  if (imu_ready == 0U)
  {
    return 0U;
  }
  if (mpu_read_bytes(MPU6050_REG_FIFO_COUNT_H, count_bytes, sizeof(count_bytes)) != HAL_OK)
  {
    imu_ready = 0U;
    g_imu_read_error_count++;
    return 0U;
  }

  fifo_bytes = (uint16_t)(((uint16_t)count_bytes[0] << 8) | count_bytes[1]);
  g_imu_last_fifo_bytes = fifo_bytes;
  if (fifo_bytes >= (MPU6050_FIFO_CAPACITY - MPU6050_FIFO_FRAME_SIZE))
  {
    (void)IMU_Sensor_ResetFifo();
    return 0U;
  }

  if (fifo_bytes < MPU6050_FIFO_FRAME_SIZE)
  {
    /* Some MPU6050 boards do not keep FIFO generation active reliably. */
    return 1U;
  }

  return (uint16_t)(fifo_bytes / MPU6050_FIFO_FRAME_SIZE);
}

bool IMU_Sensor_Read(IMU_SensorSample* sample)
{
  uint8_t count_bytes[2];
  uint8_t buffer[MPU6050_OUTPUT_FRAME_SIZE];
  uint16_t fifo_bytes;
  uint8_t gyro_offset;

  if (sample == NULL)
  {
    return false;
  }

  if ((imu_ready == 0U) ||
      (mpu_read_bytes(MPU6050_REG_FIFO_COUNT_H, count_bytes, sizeof(count_bytes)) != HAL_OK))
  {
    imu_ready = 0U;
    g_imu_read_error_count++;
    return false;
  }

  fifo_bytes = (uint16_t)(((uint16_t)count_bytes[0] << 8) | count_bytes[1]);
  g_imu_last_fifo_bytes = fifo_bytes;
  if ((fifo_bytes >= MPU6050_FIFO_FRAME_SIZE) &&
      (fifo_bytes < (MPU6050_FIFO_CAPACITY - MPU6050_FIFO_FRAME_SIZE)))
  {
    if (mpu_read_bytes(MPU6050_REG_FIFO_R_W, buffer, MPU6050_FIFO_FRAME_SIZE) != HAL_OK)
    {
      imu_ready = 0U;
      g_imu_read_error_count++;
      return false;
    }
    gyro_offset = 6U;
    g_imu_fifo_frame_count++;
  }
  else
  {
    if (fifo_bytes >= (MPU6050_FIFO_CAPACITY - MPU6050_FIFO_FRAME_SIZE))
    {
      (void)IMU_Sensor_ResetFifo();
    }
    if (mpu_read_bytes(MPU6050_REG_ACCEL_XOUT_H, buffer, MPU6050_OUTPUT_FRAME_SIZE) != HAL_OK)
    {
      imu_ready = 0U;
      g_imu_read_error_count++;
      return false;
    }
    gyro_offset = 8U;
    g_imu_direct_frame_count++;
  }

  sample->raw_accel[0] = bytes_to_i16(buffer[0], buffer[1]);
  sample->raw_accel[1] = bytes_to_i16(buffer[2], buffer[3]);
  sample->raw_accel[2] = bytes_to_i16(buffer[4], buffer[5]);
  sample->raw_gyro[0] = bytes_to_i16(buffer[gyro_offset], buffer[gyro_offset + 1U]);
  sample->raw_gyro[1] = bytes_to_i16(buffer[gyro_offset + 2U], buffer[gyro_offset + 3U]);
  sample->raw_gyro[2] = bytes_to_i16(buffer[gyro_offset + 4U], buffer[gyro_offset + 5U]);

  for (uint8_t i = 0U; i < 3U; i++)
  {
    sample->accel_g[i] = ((float)sample->raw_accel[i] * accel_scale) / MPU6050_ACCEL_LSB_PER_G;
    sample->gyro_rad_s[i] = (((float)sample->raw_gyro[i] - gyro_bias_raw[i]) /
                             MPU6050_GYRO_LSB_PER_DPS) * MPU6050_DEG_TO_RAD;
  }

  imu_ready = 1U;
  return true;
}
