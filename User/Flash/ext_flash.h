#ifndef EXT_FLASH_H
#define EXT_FLASH_H

#include <stdint.h>

#define EXT_FLASH_ASSET_BASE 0x00000000UL

uint8_t ExtFlash_Init(void);
uint8_t ExtFlash_IsReady(void);
uint8_t ExtFlash_Read(uint32_t address, uint8_t* data, uint32_t length);
uint32_t ExtFlash_GetJedecId(void);
uint8_t ExtFlash_Erase4K(uint32_t address);
uint8_t ExtFlash_PageProgram(uint32_t address, const uint8_t* data, uint32_t length);

#endif
