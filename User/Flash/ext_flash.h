#ifndef EXT_FLASH_H
#define EXT_FLASH_H

#include <stdint.h>

#define EXT_FLASH_SIZE_BYTES              0x01000000UL
#define EXT_FLASH_SECTOR_SIZE_BYTES       0x00001000UL
#define EXT_FLASH_PAGE_SIZE_BYTES         0x00000100UL

#define EXT_FLASH_ASSET_BASE              0x00000000UL
#define EXT_FLASH_ASSET_PARTITION_BYTES   0x00100000UL

#define EXT_FLASH_MOTION_META_A_BASE      0x00100000UL
#define EXT_FLASH_MOTION_META_B_BASE      0x00101000UL
#define EXT_FLASH_MOTION_DATA_BASE        0x00110000UL
#define EXT_FLASH_MOTION_DATA_BYTES       0x00800000UL
#define EXT_FLASH_MOTION_DATA_END         (EXT_FLASH_MOTION_DATA_BASE + EXT_FLASH_MOTION_DATA_BYTES)

uint8_t ExtFlash_Init(void);
uint8_t ExtFlash_IsReady(void);
uint8_t ExtFlash_Read(uint32_t address, uint8_t* data, uint32_t length);
uint32_t ExtFlash_GetJedecId(void);
uint8_t ExtFlash_Erase4K(uint32_t address);
uint8_t ExtFlash_PageProgram(uint32_t address, const uint8_t* data, uint32_t length);

#endif
