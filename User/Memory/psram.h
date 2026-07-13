#ifndef PSRAM_H
#define PSRAM_H

#include <stdint.h>

#define PSRAM_SIZE_BYTES 0x00800000UL
#define PSRAM_STATUS_NOT_RUN 0x00000000UL
#define PSRAM_STATUS_OK      0x50534F4BUL

extern volatile uint32_t g_psram_status;
extern volatile uint32_t g_psram_error;
extern volatile uint32_t g_psram_debug[4];

uint8_t PSRAM_Init(void);
uint8_t PSRAM_IsReady(void);
uint8_t PSRAM_Read(uint32_t address, uint8_t* data, uint32_t length);
uint8_t PSRAM_Write(uint32_t address, const uint8_t* data, uint32_t length);
uint8_t PSRAM_RunSelfTest(void);

#endif
