#include "eeprom.h"

#include <stddef.h>

#define EEPROM_I2C_ADDRESS_FIRST 0x50U
#define EEPROM_I2C_ADDRESS_LAST 0x57U
#define EEPROM_TRANSFER_TIMEOUT_MS 100U
#define EEPROM_READY_TRIALS 12U

#define EEPROM_STATUS_NOT_INITIALIZED 0x45500000UL
#define EEPROM_STATUS_OK 0x45504F4BUL
#define EEPROM_STATUS_NOT_FOUND 0x45504E46UL
#define EEPROM_STATUS_IO_ERROR 0x45504552UL

volatile uint32_t g_eeprom_status = EEPROM_STATUS_NOT_INITIALIZED;
volatile uint8_t g_eeprom_device_address = 0U;

static I2C_HandleTypeDef* eeprom_i2c = NULL;
static uint16_t eeprom_hal_address = 0U;
static bool eeprom_ready = false;

static bool EEPROM_AddressRangeIsValid(uint16_t address, uint16_t length)
{
  return ((uint32_t)address + (uint32_t)length) <= EEPROM_CAPACITY_BYTES;
}

static bool EEPROM_WaitUntilReady(void)
{
  if ((eeprom_i2c == NULL) || (eeprom_hal_address == 0U))
  {
    return false;
  }

  if (HAL_I2C_IsDeviceReady(eeprom_i2c, eeprom_hal_address,
                            EEPROM_READY_TRIALS, 1U) != HAL_OK)
  {
    g_eeprom_status = EEPROM_STATUS_IO_ERROR;
    return false;
  }

  return true;
}

bool EEPROM_Init(I2C_HandleTypeDef* i2c)
{
  uint8_t address;

  eeprom_i2c = i2c;
  eeprom_hal_address = 0U;
  eeprom_ready = false;
  g_eeprom_device_address = 0U;
  g_eeprom_status = EEPROM_STATUS_NOT_INITIALIZED;

  if (i2c == NULL)
  {
    g_eeprom_status = EEPROM_STATUS_NOT_FOUND;
    return false;
  }

  for (address = EEPROM_I2C_ADDRESS_FIRST;
       address <= EEPROM_I2C_ADDRESS_LAST;
       address++)
  {
    const uint16_t hal_address = (uint16_t)address << 1U;

    if (HAL_I2C_IsDeviceReady(i2c, hal_address, 3U, 10U) == HAL_OK)
    {
      eeprom_hal_address = hal_address;
      g_eeprom_device_address = address;
      eeprom_ready = true;
      g_eeprom_status = EEPROM_STATUS_OK;
      return true;
    }
  }

  g_eeprom_status = EEPROM_STATUS_NOT_FOUND;
  return false;
}

bool EEPROM_IsReady(void)
{
  return eeprom_ready;
}

bool EEPROM_Read(uint16_t address, void* data, uint16_t length)
{
  if ((length == 0U) && EEPROM_AddressRangeIsValid(address, length))
  {
    return true;
  }
  if ((!eeprom_ready) || (data == NULL) ||
      (!EEPROM_AddressRangeIsValid(address, length)))
  {
    return false;
  }

  if (HAL_I2C_Mem_Read(eeprom_i2c, eeprom_hal_address, address,
                       I2C_MEMADD_SIZE_16BIT, data, length,
                       EEPROM_TRANSFER_TIMEOUT_MS) != HAL_OK)
  {
    g_eeprom_status = EEPROM_STATUS_IO_ERROR;
    return false;
  }

  g_eeprom_status = EEPROM_STATUS_OK;
  return true;
}

bool EEPROM_Write(uint16_t address, const void* data, uint16_t length)
{
  const uint8_t* source = (const uint8_t*)data;

  if ((length == 0U) && EEPROM_AddressRangeIsValid(address, length))
  {
    return true;
  }
  if ((!eeprom_ready) || (data == NULL) ||
      (!EEPROM_AddressRangeIsValid(address, length)))
  {
    return false;
  }

  while (length > 0U)
  {
    const uint16_t page_offset = address % EEPROM_PAGE_SIZE_BYTES;
    const uint16_t page_remaining = EEPROM_PAGE_SIZE_BYTES - page_offset;
    const uint16_t chunk = (length < page_remaining) ? length : page_remaining;

    if (HAL_I2C_Mem_Write(eeprom_i2c, eeprom_hal_address, address,
                          I2C_MEMADD_SIZE_16BIT, (uint8_t*)source, chunk,
                          EEPROM_TRANSFER_TIMEOUT_MS) != HAL_OK)
    {
      g_eeprom_status = EEPROM_STATUS_IO_ERROR;
      return false;
    }
    if (!EEPROM_WaitUntilReady())
    {
      return false;
    }

    address = (uint16_t)(address + chunk);
    source += chunk;
    length = (uint16_t)(length - chunk);
  }

  g_eeprom_status = EEPROM_STATUS_OK;
  return true;
}
