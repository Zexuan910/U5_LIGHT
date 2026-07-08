#include "imu_sensor.h"

#include "main.h"

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
#define MPU6050_FIFO_CAPACITY        1024U
#define MPU6050_I2C_TIMEOUT_MS       20U
#define MPU6050_ACCEL_LSB_PER_G      16384.0f
#define MPU6050_GYRO_LSB_PER_DPS     65.5f
#define MPU6050_DEG_TO_RAD           0.0174532925f

static uint8_t imu_ready = 0U;
static uint16_t imu_addr = (MPU6050_ADDR_PRIMARY_7BIT << 1);

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
  (void)mpu_write_reg(MPU6050_REG_FIFO_EN, 0x78U);
  (void)mpu_write_reg(MPU6050_REG_USER_CTRL, 0x44U);

  imu_ready = 1U;
}

bool IMU_Sensor_IsReady(void)
{
  return imu_ready != 0U;
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

  if ((imu_ready == 0U) ||
      (mpu_read_bytes(MPU6050_REG_FIFO_COUNT_H, count_bytes, sizeof(count_bytes)) != HAL_OK))
  {
    return 0U;
  }

  fifo_bytes = (uint16_t)(((uint16_t)count_bytes[0] << 8) | count_bytes[1]);
  if (fifo_bytes >= (MPU6050_FIFO_CAPACITY - MPU6050_FIFO_FRAME_SIZE))
  {
    (void)IMU_Sensor_ResetFifo();
    return 0U;
  }

  return (uint16_t)(fifo_bytes / MPU6050_FIFO_FRAME_SIZE);
}

bool IMU_Sensor_Read(IMU_SensorSample* sample)
{
  uint8_t buffer[MPU6050_FIFO_FRAME_SIZE];

  if (sample == NULL)
  {
    return false;
  }

  if (mpu_read_bytes(MPU6050_REG_FIFO_R_W, buffer, sizeof(buffer)) != HAL_OK)
  {
    imu_ready = 0U;
    return false;
  }

  sample->raw_accel[0] = bytes_to_i16(buffer[0], buffer[1]);
  sample->raw_accel[1] = bytes_to_i16(buffer[2], buffer[3]);
  sample->raw_accel[2] = bytes_to_i16(buffer[4], buffer[5]);
  sample->raw_gyro[0] = bytes_to_i16(buffer[6], buffer[7]);
  sample->raw_gyro[1] = bytes_to_i16(buffer[8], buffer[9]);
  sample->raw_gyro[2] = bytes_to_i16(buffer[10], buffer[11]);

  for (uint8_t i = 0U; i < 3U; i++)
  {
    sample->accel_g[i] = (float)sample->raw_accel[i] / MPU6050_ACCEL_LSB_PER_G;
    sample->gyro_rad_s[i] = ((float)sample->raw_gyro[i] / MPU6050_GYRO_LSB_PER_DPS) * MPU6050_DEG_TO_RAD;
  }

  imu_ready = 1U;
  return true;
}
