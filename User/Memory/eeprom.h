#ifndef EEPROM_H
#define EEPROM_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32u5xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EEPROM_CAPACITY_BYTES 8192U
#define EEPROM_PAGE_SIZE_BYTES 32U

extern volatile uint32_t g_eeprom_status;
extern volatile uint8_t g_eeprom_device_address;

bool EEPROM_Init(I2C_HandleTypeDef* i2c);
bool EEPROM_IsReady(void);
bool EEPROM_Read(uint16_t address, void* data, uint16_t length);
bool EEPROM_Write(uint16_t address, const void* data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif
