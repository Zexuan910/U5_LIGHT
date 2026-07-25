#include "mpu6050.h"

#include "main.h"

#include <string.h>

#define MPU6050_ADDR_LOW_7BIT       0x68U
#define MPU6050_ADDR_HIGH_7BIT      0x69U
#define MPU6050_REG_SMPLRT_DIV      0x19U
#define MPU6050_REG_CONFIG          0x1AU
#define MPU6050_REG_GYRO_CONFIG     0x1BU
#define MPU6050_REG_ACCEL_CONFIG    0x1CU
#define MPU6050_REG_ACCEL_XOUT_H    0x3BU
#define MPU6050_REG_PWR_MGMT_1      0x6BU
#define MPU6050_REG_WHO_AM_I        0x75U
#define MPU6050_WHO_AM_I_VALUE      0x68U
#define MPU6050_I2C_TIMEOUT_MS      20U
#define MPU6050_I2C_READY_TRIALS    2U

static I2C_HandleTypeDef s_hi2c2;
static uint8_t s_i2cReady;
static Mpu6050Snapshot s_snapshot;

static int16_t bytes_to_i16(uint8_t high_byte, uint8_t low_byte)
{
  return (int16_t)(((uint16_t)high_byte << 8) | (uint16_t)low_byte);
}

static int16_t scale_float_to_i16(float value)
{
  if (value > 32767.0f) {
    return 32767;
  }
  if (value < -32768.0f) {
    return -32768;
  }
  if (value >= 0.0f) {
    return (int16_t)(value + 0.5f);
  }
  return (int16_t)(value - 0.5f);
}

static void update_int_level(void)
{
  s_snapshot.intLevel = (uint8_t)HAL_GPIO_ReadPin(MPU6050_INT_GPIO_Port, MPU6050_INT_Pin);
}

static void init_int_gpio(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitStruct.Pin = MPU6050_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MPU6050_INT_GPIO_Port, &GPIO_InitStruct);
}

static HAL_StatusTypeDef init_i2c2(void)
{
  HAL_StatusTypeDef halStatus;

  if (s_i2cReady != 0U) {
    return HAL_OK;
  }

  s_hi2c2.Instance = I2C2;
  s_hi2c2.Init.Timing = 0x00000E14;
  s_hi2c2.Init.OwnAddress1 = 0;
  s_hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  s_hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  s_hi2c2.Init.OwnAddress2 = 0;
  s_hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  s_hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  s_hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  halStatus = HAL_I2C_Init(&s_hi2c2);
  if (halStatus != HAL_OK) {
    return halStatus;
  }

  halStatus = HAL_I2CEx_ConfigAnalogFilter(&s_hi2c2, I2C_ANALOGFILTER_ENABLE);
  if (halStatus != HAL_OK) {
    return halStatus;
  }

  halStatus = HAL_I2CEx_ConfigDigitalFilter(&s_hi2c2, 0);
  if (halStatus != HAL_OK) {
    return halStatus;
  }

  s_i2cReady = 1U;
  return HAL_OK;
}

static HAL_StatusTypeDef read_reg(uint8_t reg, uint8_t *value)
{
  HAL_StatusTypeDef halStatus = HAL_I2C_Mem_Read(&s_hi2c2,
                                                 (uint16_t)(s_snapshot.address7bit << 1),
                                                 reg,
                                                 I2C_MEMADD_SIZE_8BIT,
                                                 value,
                                                 1U,
                                                 MPU6050_I2C_TIMEOUT_MS);
  s_snapshot.lastHalStatus = (int32_t)halStatus;
  s_snapshot.i2cError = HAL_I2C_GetError(&s_hi2c2);
  return halStatus;
}

static HAL_StatusTypeDef write_reg(uint8_t reg, uint8_t value)
{
  HAL_StatusTypeDef halStatus = HAL_I2C_Mem_Write(&s_hi2c2,
                                                  (uint16_t)(s_snapshot.address7bit << 1),
                                                  reg,
                                                  I2C_MEMADD_SIZE_8BIT,
                                                  &value,
                                                  1U,
                                                  MPU6050_I2C_TIMEOUT_MS);
  s_snapshot.lastHalStatus = (int32_t)halStatus;
  s_snapshot.i2cError = HAL_I2C_GetError(&s_hi2c2);
  return halStatus;
}

static uint8_t probe_address(uint8_t address7bit)
{
  HAL_StatusTypeDef halStatus = HAL_I2C_IsDeviceReady(&s_hi2c2,
                                                      (uint16_t)(address7bit << 1),
                                                      MPU6050_I2C_READY_TRIALS,
                                                      MPU6050_I2C_TIMEOUT_MS);
  s_snapshot.lastHalStatus = (int32_t)halStatus;
  s_snapshot.i2cError = HAL_I2C_GetError(&s_hi2c2);
  return (halStatus == HAL_OK) ? 1U : 0U;
}

static uint8_t configure_sensor(void)
{
  if (write_reg(MPU6050_REG_PWR_MGMT_1, 0x01U) != HAL_OK) {
    return 0U;
  }
  HAL_Delay(10U);
  if (write_reg(MPU6050_REG_SMPLRT_DIV, 0x04U) != HAL_OK) {
    return 0U;
  }
  if (write_reg(MPU6050_REG_CONFIG, 0x03U) != HAL_OK) {
    return 0U;
  }
  if (write_reg(MPU6050_REG_GYRO_CONFIG, 0x00U) != HAL_OK) {
    return 0U;
  }
  if (write_reg(MPU6050_REG_ACCEL_CONFIG, 0x00U) != HAL_OK) {
    return 0U;
  }
  return 1U;
}

const Mpu6050Snapshot *Mpu6050_Init(void)
{
  const uint8_t candidates[] = {MPU6050_ADDR_LOW_7BIT, MPU6050_ADDR_HIGH_7BIT};
  HAL_StatusTypeDef halStatus;

  memset(&s_snapshot, 0, sizeof(s_snapshot));
  s_snapshot.status = MPU6050_STATUS_IDLE;
  init_int_gpio();
  update_int_level();

  halStatus = init_i2c2();
  s_snapshot.lastHalStatus = (int32_t)halStatus;
  if (halStatus != HAL_OK) {
    s_snapshot.status = MPU6050_STATUS_I2C_INIT_FAIL;
    s_snapshot.i2cError = HAL_I2C_GetError(&s_hi2c2);
    return &s_snapshot;
  }

  for (uint8_t i = 0U; i < (uint8_t)(sizeof(candidates) / sizeof(candidates[0])); ++i) {
    if (probe_address(candidates[i]) != 0U) {
      s_snapshot.address7bit = candidates[i];
      break;
    }
  }

  if (s_snapshot.address7bit == 0U) {
    s_snapshot.status = MPU6050_STATUS_NOT_FOUND;
    return &s_snapshot;
  }

  if (read_reg(MPU6050_REG_WHO_AM_I, &s_snapshot.whoAmI) != HAL_OK) {
    s_snapshot.status = MPU6050_STATUS_WHOAMI_FAIL;
    return &s_snapshot;
  }

  if (s_snapshot.whoAmI != MPU6050_WHO_AM_I_VALUE) {
    s_snapshot.status = MPU6050_STATUS_WHOAMI_FAIL;
    return &s_snapshot;
  }

  if (configure_sensor() == 0U) {
    s_snapshot.status = MPU6050_STATUS_CONFIG_FAIL;
    return &s_snapshot;
  }

  s_snapshot.status = MPU6050_STATUS_READY;
  return &s_snapshot;
}

const Mpu6050Snapshot *Mpu6050_Read(void)
{
  uint8_t buffer[14];
  HAL_StatusTypeDef halStatus;
  const float accelScale = 1.0f / 16384.0f;
  const float gyroScale = 0.01745329252f / 131.0f;

  update_int_level();
  if ((s_snapshot.status != MPU6050_STATUS_READY) && (s_snapshot.status != MPU6050_STATUS_READ_FAIL)) {
    s_snapshot.readFailCount++;
    return &s_snapshot;
  }

  halStatus = HAL_I2C_Mem_Read(&s_hi2c2,
                               (uint16_t)(s_snapshot.address7bit << 1),
                               MPU6050_REG_ACCEL_XOUT_H,
                               I2C_MEMADD_SIZE_8BIT,
                               buffer,
                               sizeof(buffer),
                               MPU6050_I2C_TIMEOUT_MS);
  s_snapshot.lastHalStatus = (int32_t)halStatus;
  s_snapshot.i2cError = HAL_I2C_GetError(&s_hi2c2);
  if (halStatus != HAL_OK) {
    s_snapshot.status = MPU6050_STATUS_READ_FAIL;
    s_snapshot.sampleReady = 0U;
    s_snapshot.readFailCount++;
    return &s_snapshot;
  }

  s_snapshot.rawAccel[0] = bytes_to_i16(buffer[0], buffer[1]);
  s_snapshot.rawAccel[1] = bytes_to_i16(buffer[2], buffer[3]);
  s_snapshot.rawAccel[2] = bytes_to_i16(buffer[4], buffer[5]);
  s_snapshot.rawTemp = bytes_to_i16(buffer[6], buffer[7]);
  s_snapshot.rawGyro[0] = bytes_to_i16(buffer[8], buffer[9]);
  s_snapshot.rawGyro[1] = bytes_to_i16(buffer[10], buffer[11]);
  s_snapshot.rawGyro[2] = bytes_to_i16(buffer[12], buffer[13]);

  for (uint8_t i = 0U; i < 3U; ++i) {
    s_snapshot.accelG[i] = (float)s_snapshot.rawAccel[i] * accelScale;
    s_snapshot.gyroRadS[i] = (float)s_snapshot.rawGyro[i] * gyroScale;
    s_snapshot.accelMg[i] = scale_float_to_i16(s_snapshot.accelG[i] * 1000.0f);
    s_snapshot.gyroMdps[i] = scale_float_to_i16(s_snapshot.gyroRadS[i] * 57295.7795f);
  }

  s_snapshot.temperatureX10 = scale_float_to_i16(((float)s_snapshot.rawTemp / 34.0f + 36.53f) * 10.0f);
  s_snapshot.status = MPU6050_STATUS_READY;
  s_snapshot.sampleReady = 1U;
  s_snapshot.readCount++;
  s_snapshot.lastReadTick = HAL_GetTick();
  return &s_snapshot;
}

const Mpu6050Snapshot *Mpu6050_GetSnapshot(void)
{
  update_int_level();
  return &s_snapshot;
}

const char *Mpu6050_StatusName(Mpu6050Status status)
{
  switch (status) {
  case MPU6050_STATUS_IDLE:
    return "IDLE";
  case MPU6050_STATUS_I2C_INIT_FAIL:
    return "I2C_INIT_FAIL";
  case MPU6050_STATUS_NOT_FOUND:
    return "NOT_FOUND";
  case MPU6050_STATUS_WHOAMI_FAIL:
    return "WHOAMI_FAIL";
  case MPU6050_STATUS_CONFIG_FAIL:
    return "CONFIG_FAIL";
  case MPU6050_STATUS_READY:
    return "READY";
  case MPU6050_STATUS_READ_FAIL:
    return "READ_FAIL";
  default:
    return "UNKNOWN";
  }
}
