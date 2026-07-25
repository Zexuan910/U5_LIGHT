#include "psram.h"

#include "main.h"
#include "stm32u5xx_hal.h"

#include <string.h>

#define APS6404_CMD_RESET_ENABLE 0x66U
#define APS6404_CMD_RESET        0x99U
#define APS6404_CMD_READ         0x03U
#define APS6404_CMD_WRITE        0x02U

#define PSRAM_TEST_LENGTH 256U

volatile uint32_t g_psram_status = PSRAM_STATUS_NOT_RUN;
volatile uint32_t g_psram_error = 0UL;
volatile uint32_t g_psram_debug[4] = {0UL, 0UL, 0UL, 0UL};

static uint8_t psram_ready = 0U;

static void PSRAM_Delay(void)
{
    __NOP();
    __NOP();
}

static void PSRAM_SetError(uint32_t code)
{
    g_psram_error = code;
    g_psram_status = code;
    psram_ready = 0U;
}

static void PSRAM_SetCS(GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) {
        PSRAM_CS_GPIO_Port->BSRR = PSRAM_CS_Pin;
    } else {
        PSRAM_CS_GPIO_Port->BSRR = ((uint32_t)PSRAM_CS_Pin << 16);
    }
}

static void PSRAM_SetCLK(GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) {
        FLASH_CLK_GPIO_Port->BSRR = FLASH_CLK_Pin;
    } else {
        FLASH_CLK_GPIO_Port->BSRR = ((uint32_t)FLASH_CLK_Pin << 16);
    }
}

static void PSRAM_SetMOSI(GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) {
        FLASH_IO0_GPIO_Port->BSRR = FLASH_IO0_Pin;
    } else {
        FLASH_IO0_GPIO_Port->BSRR = ((uint32_t)FLASH_IO0_Pin << 16);
    }
}

static uint8_t PSRAM_ReadMISO(void)
{
    return ((FLASH_IO1_GPIO_Port->IDR & FLASH_IO1_Pin) != 0U) ? 1U : 0U;
}

static void PSRAM_Select(void)
{
    PSRAM_SetCLK(GPIO_PIN_RESET);
    PSRAM_SetCS(GPIO_PIN_RESET);
    PSRAM_Delay();
}

static void PSRAM_Deselect(void)
{
    PSRAM_Delay();
    PSRAM_SetCS(GPIO_PIN_SET);
    PSRAM_SetCLK(GPIO_PIN_RESET);
}

static void PSRAM_WriteByte(uint8_t value)
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++) {
        PSRAM_SetMOSI((value & 0x80U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
        PSRAM_Delay();
        PSRAM_SetCLK(GPIO_PIN_SET);
        PSRAM_Delay();
        PSRAM_SetCLK(GPIO_PIN_RESET);
        value <<= 1;
    }
}

static uint8_t PSRAM_ReadByte(void)
{
    uint8_t value = 0U;
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++) {
        value <<= 1;
        PSRAM_Delay();
        PSRAM_SetCLK(GPIO_PIN_SET);
        PSRAM_Delay();
        value |= PSRAM_ReadMISO();
        PSRAM_SetCLK(GPIO_PIN_RESET);
    }

    return value;
}

static void PSRAM_WriteAddress(uint32_t address)
{
    PSRAM_WriteByte((uint8_t)(address >> 16));
    PSRAM_WriteByte((uint8_t)(address >> 8));
    PSRAM_WriteByte((uint8_t)address);
}

static void PSRAM_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(PSRAM_CS_GPIO_Port, PSRAM_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(FLASH_CLK_GPIO_Port, FLASH_CLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FLASH_IO0_GPIO_Port, FLASH_IO0_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FLASH_IO2_GPIO_Port, FLASH_IO2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(FLASH_IO3_GPIO_Port, FLASH_IO3_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = PSRAM_CS_Pin | FLASH_CLK_Pin | FLASH_IO2_Pin | FLASH_IO3_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = FLASH_IO0_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FLASH_IO0_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = FLASH_IO1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(FLASH_IO1_GPIO_Port, &GPIO_InitStruct);
}

static void PSRAM_SendSimpleCommand(uint8_t instruction)
{
    PSRAM_Select();
    PSRAM_WriteByte(instruction);
    PSRAM_Deselect();
}

static uint8_t PSRAM_ResetToSpiMode(void)
{
    HAL_Delay(1U);
    PSRAM_SendSimpleCommand(APS6404_CMD_RESET_ENABLE);
    PSRAM_SendSimpleCommand(APS6404_CMD_RESET);
    HAL_Delay(1U);
    return 1U;
}

uint8_t PSRAM_Read(uint32_t address, uint8_t* data, uint32_t length)
{
    uint32_t i;

    if ((psram_ready == 0U) || (data == 0) || (length == 0U)) {
        return 0U;
    }
    if ((address >= PSRAM_SIZE_BYTES) || ((PSRAM_SIZE_BYTES - address) < length)) {
        return 0U;
    }

    PSRAM_Select();
    PSRAM_WriteByte(APS6404_CMD_READ);
    PSRAM_WriteAddress(address);
    for (i = 0UL; i < length; i++) {
        data[i] = PSRAM_ReadByte();
    }
    PSRAM_Deselect();

    return 1U;
}

uint8_t PSRAM_Write(uint32_t address, const uint8_t* data, uint32_t length)
{
    uint32_t offset = 0UL;

    if ((psram_ready == 0U) || (data == 0) || (length == 0U)) {
        return 0U;
    }
    if ((address >= PSRAM_SIZE_BYTES) || ((PSRAM_SIZE_BYTES - address) < length)) {
        return 0U;
    }

    while (offset < length) {
        uint32_t chunk = length - offset;
        uint32_t page_left = 1024UL - ((address + offset) & 1023UL);
        if (chunk > page_left) {
            chunk = page_left;
        }

        PSRAM_Select();
        PSRAM_WriteByte(APS6404_CMD_WRITE);
        PSRAM_WriteAddress(address + offset);
        for (uint32_t i = 0UL; i < chunk; i++) {
            PSRAM_WriteByte(data[offset + i]);
        }
        PSRAM_Deselect();
        offset += chunk;
    }

    return 1U;
}

static uint8_t PSRAM_TestAt(uint32_t address, uint8_t salt)
{
    uint8_t write_buf[PSRAM_TEST_LENGTH];
    uint8_t read_buf[PSRAM_TEST_LENGTH];
    uint32_t i;

    for (i = 0UL; i < PSRAM_TEST_LENGTH; i++) {
        write_buf[i] = (uint8_t)((i * 37UL) ^ (i >> 1) ^ salt);
        read_buf[i] = 0U;
    }
    g_psram_debug[0] = address;
    g_psram_debug[1] = 0xFFFFFFFFUL;
    g_psram_debug[2] = 0UL;
    g_psram_debug[3] = 0UL;

    if (PSRAM_Write(address, write_buf, PSRAM_TEST_LENGTH) == 0U) {
        return 0U;
    }
    if (PSRAM_Read(address, read_buf, PSRAM_TEST_LENGTH) == 0U) {
        return 0U;
    }

    for (i = 0UL; i < PSRAM_TEST_LENGTH; i++) {
        if (write_buf[i] != read_buf[i]) {
            g_psram_debug[1] = i;
            g_psram_debug[2] = ((uint32_t)write_buf[i]) | ((uint32_t)read_buf[i] << 8);
            g_psram_debug[3] = ((uint32_t)write_buf[0]) |
                               ((uint32_t)write_buf[1] << 8) |
                               ((uint32_t)read_buf[0] << 16) |
                               ((uint32_t)read_buf[1] << 24);
            return 0U;
        }
    }

    return 1U;
}

uint8_t PSRAM_RunSelfTest(void)
{
    if (psram_ready == 0U) {
        PSRAM_SetError(0x50534530UL);
        return 0U;
    }
    if (PSRAM_TestAt(0x000000UL, 0x21U) == 0U) {
        PSRAM_SetError(0x50534531UL);
        return 0U;
    }
    if (PSRAM_TestAt(0x001000UL, 0x42U) == 0U) {
        PSRAM_SetError(0x50534532UL);
        return 0U;
    }
    if (PSRAM_TestAt(PSRAM_SIZE_BYTES - PSRAM_TEST_LENGTH, 0x84U) == 0U) {
        PSRAM_SetError(0x50534533UL);
        return 0U;
    }

    g_psram_error = 0UL;
    g_psram_status = PSRAM_STATUS_OK;
    return 1U;
}

uint8_t PSRAM_Init(void)
{
    g_psram_status = PSRAM_STATUS_NOT_RUN;
    g_psram_error = 0UL;
    g_psram_debug[0] = 0UL;
    g_psram_debug[1] = 0UL;
    g_psram_debug[2] = 0UL;
    g_psram_debug[3] = 0UL;
    psram_ready = 0U;

    PSRAM_GPIO_Init();
    if (PSRAM_ResetToSpiMode() == 0U) {
        PSRAM_SetError(0x50534933UL);
        return 0U;
    }
    psram_ready = 1U;
    return PSRAM_RunSelfTest();
}

uint8_t PSRAM_IsReady(void)
{
    return psram_ready;
}
