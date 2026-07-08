#include "cst816t.h"

extern I2C_HandleTypeDef hi2c3;

#define CST816T_ADDR_7BIT          0x15U
#define CST816T_ADDR               (CST816T_ADDR_7BIT << 1)
#define CST816T_REG_GESTURE_ID     0x01U
#define CST816T_REG_FINGER_NUM     0x02U
#define CST816T_REG_CHIP_ID        0xA7U
#define CST816T_REG_LONG_PRESS     0xEBU
#define CST816T_REG_AUTO_SLEEP     0xFEU
#define CST816T_I2C_TIMEOUT_MS     50U

static UBYTE cst816t_connected = 0U;

static HAL_StatusTypeDef CST816T_ReadReg(UBYTE reg, UBYTE* data, UWORD len)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          CST816T_ADDR,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          len,
                          CST816T_I2C_TIMEOUT_MS);
}

static void CST816T_WriteReg(UBYTE reg, UBYTE value)
{
  (void)HAL_I2C_Mem_Write(&hi2c3,
                          CST816T_ADDR,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          &value,
                          1U,
                          CST816T_I2C_TIMEOUT_MS);
}

static UBYTE CST816T_ReadPointRaw(UWORD* x, UWORD* y, UBYTE* finger)
{
  UBYTE chip_id = 0U;
  UBYTE buf[5] = {0U, 0U, 0U, 0U, 0U};
  UWORD raw_x;
  UWORD raw_y;

  if ((x == NULL) || (y == NULL))
  {
    return 0U;
  }

  if (cst816t_connected == 0U)
  {
    if (CST816T_ReadReg(CST816T_REG_CHIP_ID, &chip_id, 1U) != HAL_OK)
    {
      return 0U;
    }
    cst816t_connected = 1U;
  }

  if (CST816T_ReadReg(CST816T_REG_FINGER_NUM, buf, sizeof(buf)) != HAL_OK)
  {
    cst816t_connected = 0U;
    return 0U;
  }

  raw_x = (UWORD)((((UWORD)buf[1] & 0x0FU) << 8) | buf[2]);
  raw_y = (UWORD)((((UWORD)buf[3] & 0x0FU) << 8) | buf[4]);

  if ((raw_x >= CST816T_WIDTH) && (raw_x < CST816T_HEIGHT) &&
      (raw_y < CST816T_WIDTH))
  {
    UWORD swapped = raw_x;
    raw_x = raw_y;
    raw_y = swapped;
  }

  if ((raw_x >= CST816T_WIDTH) || (raw_y >= CST816T_HEIGHT))
  {
    return 0U;
  }

  *x = raw_x;
  *y = raw_y;
  if (finger != NULL)
  {
    *finger = (UBYTE)(buf[0] & 0x0FU);
  }
  cst816t_connected = 1U;
  return 1U;
}

void CST816T_Init(void)
{
  UBYTE chip_id = 0U;

  HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(10U);
  HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(80U);

  if (CST816T_ReadReg(CST816T_REG_CHIP_ID, &chip_id, 1U) == HAL_OK)
  {
    cst816t_connected = 1U;
    CST816T_WriteReg(CST816T_REG_AUTO_SLEEP, 0xFFU);
    CST816T_WriteReg(CST816T_REG_LONG_PRESS, 0x01U);
  }
  else
  {
    cst816t_connected = 0U;
  }
}

UBYTE CST816T_IsConnected(void)
{
  return cst816t_connected;
}

void CST816T_KeepAwake(void)
{
  CST816T_WriteReg(CST816T_REG_AUTO_SLEEP, 0xFFU);
  CST816T_WriteReg(CST816T_REG_LONG_PRESS, 0x01U);
}

UBYTE CST816T_ReadTouch(UWORD* x, UWORD* y)
{
  UBYTE finger = 0U;

  if (CST816T_ReadPointRaw(x, y, &finger) == 0U)
  {
    return 0U;
  }

  if ((finger == 0U) || (finger > 2U))
  {
    return 0U;
  }

  return 1U;
}

UBYTE CST816T_ReadTouchLoose(UWORD* x, UWORD* y)
{
  return CST816T_ReadPointRaw(x, y, NULL);
}

UBYTE CST816T_ReadGesture(UBYTE* gesture)
{
  UBYTE value = 0U;

  if (gesture == NULL)
  {
    return 0U;
  }

  if (CST816T_ReadReg(CST816T_REG_GESTURE_ID, &value, 1U) != HAL_OK)
  {
    cst816t_connected = 0U;
    return 0U;
  }

  *gesture = value;
  cst816t_connected = 1U;
  return 1U;
}

UBYTE CST816T_ReadActivity(void)
{
  UBYTE buf[6] = {0U, 0U, 0U, 0U, 0U, 0U};
  UBYTE gesture;
  UBYTE finger;

  if (CST816T_ReadReg(CST816T_REG_GESTURE_ID, buf, sizeof(buf)) != HAL_OK)
  {
    return 0U;
  }

  cst816t_connected = 1U;
  gesture = buf[0];
  finger = (UBYTE)(buf[1] & 0x0FU);

  return ((gesture != 0U) || ((finger > 0U) && (finger <= 2U))) ? 1U : 0U;
}

UBYTE CST816T_ReadDebug(UBYTE* chip_id, UBYTE* finger, UWORD* x, UWORD* y)
{
  UBYTE buf[5] = {0U, 0U, 0U, 0U, 0U};
  UBYTE id = 0U;
  UBYTE ok = 0U;
  UWORD raw_x = 0U;
  UWORD raw_y = 0U;

  if (CST816T_ReadReg(CST816T_REG_CHIP_ID, &id, 1U) == HAL_OK)
  {
    ok = 1U;
  }

  if (CST816T_ReadReg(CST816T_REG_FINGER_NUM, buf, sizeof(buf)) == HAL_OK)
  {
    ok = 1U;
    raw_x = (UWORD)((((UWORD)buf[1] & 0x0FU) << 8) | buf[2]);
    raw_y = (UWORD)((((UWORD)buf[3] & 0x0FU) << 8) | buf[4]);
  }

  if (chip_id != NULL)
  {
    *chip_id = id;
  }
  if (finger != NULL)
  {
    *finger = buf[0];
  }
  if (x != NULL)
  {
    *x = raw_x;
  }
  if (y != NULL)
  {
    *y = raw_y;
  }

  return ok;
}
