#include "ext_flash.h"

#include "main.h"
#include "stm32u5xx_hal.h"

#define GD25Q128E_JEDEC_ID 0xC84018UL
#define GD25Q128E_CMD_WRITE_ENABLE 0x06U
#define GD25Q128E_CMD_READ_STATUS1 0x05U
#define GD25Q128E_CMD_PAGE_PROGRAM 0x02U
#define GD25Q128E_CMD_SECTOR_ERASE_4K 0x20U
#define GD25Q128E_CMD_READ_DATA 0x03U
#define GD25Q128E_CMD_READ_JEDEC_ID 0x9FU
#define GD25Q128E_CMD_RELEASE_POWER_DOWN 0xABU
#define GD25Q128E_SIZE_BYTES 0x01000000UL
#define GD25Q128E_PAGE_SIZE 256UL
#define GD25Q128E_SECTOR_SIZE 4096UL
#define GD25Q128E_SR1_WIP 0x01U
#define GD25Q128E_SR1_WEL 0x02U

static uint8_t ext_flash_ready = 0U;
static uint32_t ext_flash_jedec_id = 0UL;

static void ExtFlash_Delay(void)
{
    __NOP();
}

static void ExtFlash_SetCS(GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) {
        FLASH_CS_GPIO_Port->BSRR = FLASH_CS_Pin;
    } else {
        FLASH_CS_GPIO_Port->BSRR = ((uint32_t)FLASH_CS_Pin << 16);
    }
}

static void ExtFlash_SetCLK(GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) {
        FLASH_CLK_GPIO_Port->BSRR = FLASH_CLK_Pin;
    } else {
        FLASH_CLK_GPIO_Port->BSRR = ((uint32_t)FLASH_CLK_Pin << 16);
    }
}

static void ExtFlash_SetMOSI(GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) {
        FLASH_IO0_GPIO_Port->BSRR = FLASH_IO0_Pin;
    } else {
        FLASH_IO0_GPIO_Port->BSRR = ((uint32_t)FLASH_IO0_Pin << 16);
    }
}

static uint8_t ExtFlash_ReadMISO(void)
{
    return ((FLASH_IO1_GPIO_Port->IDR & FLASH_IO1_Pin) != 0U) ? 1U : 0U;
}

static void ExtFlash_Select(void)
{
    ExtFlash_SetCLK(GPIO_PIN_RESET);
    ExtFlash_SetCS(GPIO_PIN_RESET);
    ExtFlash_Delay();
}

static void ExtFlash_Deselect(void)
{
    ExtFlash_Delay();
    ExtFlash_SetCS(GPIO_PIN_SET);
    ExtFlash_SetCLK(GPIO_PIN_RESET);
}

static void ExtFlash_WriteByte(uint8_t value)
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++) {
        ExtFlash_SetMOSI((value & 0x80U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
        ExtFlash_Delay();
        ExtFlash_SetCLK(GPIO_PIN_SET);
        ExtFlash_Delay();
        ExtFlash_SetCLK(GPIO_PIN_RESET);
        value <<= 1;
    }
}

static uint8_t ExtFlash_ReadByte(void)
{
    uint8_t value = 0U;
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++) {
        value <<= 1;
        ExtFlash_Delay();
        ExtFlash_SetCLK(GPIO_PIN_SET);
        ExtFlash_Delay();
        value |= ExtFlash_ReadMISO();
        ExtFlash_SetCLK(GPIO_PIN_RESET);
    }

    return value;
}

static void ExtFlash_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(FLASH_CLK_GPIO_Port, FLASH_CLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FLASH_IO0_GPIO_Port, FLASH_IO0_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(FLASH_IO2_GPIO_Port, FLASH_IO2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(FLASH_IO3_GPIO_Port, FLASH_IO3_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = FLASH_CS_Pin | FLASH_CLK_Pin | FLASH_IO2_Pin | FLASH_IO3_Pin;
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

static void ExtFlash_ReleasePowerDown(void)
{
    ExtFlash_Select();
    ExtFlash_WriteByte(GD25Q128E_CMD_RELEASE_POWER_DOWN);
    ExtFlash_Deselect();
    HAL_Delay(1U);
}

static uint32_t ExtFlash_ReadJedecId(void)
{
    uint32_t id;

    ExtFlash_Select();
    ExtFlash_WriteByte(GD25Q128E_CMD_READ_JEDEC_ID);
    id = ((uint32_t)ExtFlash_ReadByte()) << 16;
    id |= ((uint32_t)ExtFlash_ReadByte()) << 8;
    id |= (uint32_t)ExtFlash_ReadByte();
    ExtFlash_Deselect();

    return id;
}

static uint8_t ExtFlash_ReadStatus1(void)
{
    uint8_t status;

    ExtFlash_Select();
    ExtFlash_WriteByte(GD25Q128E_CMD_READ_STATUS1);
    status = ExtFlash_ReadByte();
    ExtFlash_Deselect();

    return status;
}

static uint8_t ExtFlash_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((ExtFlash_ReadStatus1() & GD25Q128E_SR1_WIP) != 0U) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            return 0U;
        }
    }

    return 1U;
}

static uint8_t ExtFlash_WriteEnable(void)
{
    ExtFlash_Select();
    ExtFlash_WriteByte(GD25Q128E_CMD_WRITE_ENABLE);
    ExtFlash_Deselect();

    return ((ExtFlash_ReadStatus1() & GD25Q128E_SR1_WEL) != 0U) ? 1U : 0U;
}

uint8_t ExtFlash_Init(void)
{
    ExtFlash_GPIO_Init();
    ExtFlash_ReleasePowerDown();
    ext_flash_jedec_id = ExtFlash_ReadJedecId();
    ext_flash_ready = (ext_flash_jedec_id == GD25Q128E_JEDEC_ID) ? 1U : 0U;
    return ext_flash_ready;
}

uint8_t ExtFlash_IsReady(void)
{
    return ext_flash_ready;
}

uint8_t ExtFlash_Read(uint32_t address, uint8_t* data, uint32_t length)
{
    uint32_t i;

    if ((ext_flash_ready == 0U) || (data == 0) || (length == 0U)) {
        return 0U;
    }

    if ((address >= GD25Q128E_SIZE_BYTES) || ((GD25Q128E_SIZE_BYTES - address) < length)) {
        return 0U;
    }

    ExtFlash_Select();
    ExtFlash_WriteByte(GD25Q128E_CMD_READ_DATA);
    ExtFlash_WriteByte((uint8_t)(address >> 16));
    ExtFlash_WriteByte((uint8_t)(address >> 8));
    ExtFlash_WriteByte((uint8_t)address);

    for (i = 0U; i < length; i++) {
        data[i] = ExtFlash_ReadByte();
    }

    ExtFlash_Deselect();
    return 1U;
}

uint32_t ExtFlash_GetJedecId(void)
{
    return ext_flash_jedec_id;
}

uint8_t ExtFlash_Erase4K(uint32_t address)
{
    if ((ext_flash_ready == 0U) || (address >= GD25Q128E_SIZE_BYTES)) {
        return 0U;
    }

    address &= ~(GD25Q128E_SECTOR_SIZE - 1UL);

    if (ExtFlash_WriteEnable() == 0U) {
        return 0U;
    }

    ExtFlash_Select();
    ExtFlash_WriteByte(GD25Q128E_CMD_SECTOR_ERASE_4K);
    ExtFlash_WriteByte((uint8_t)(address >> 16));
    ExtFlash_WriteByte((uint8_t)(address >> 8));
    ExtFlash_WriteByte((uint8_t)address);
    ExtFlash_Deselect();

    return ExtFlash_WaitReady(1000U);
}

uint8_t ExtFlash_PageProgram(uint32_t address, const uint8_t* data, uint32_t length)
{
    uint32_t i;
    uint32_t page_remaining;

    if ((ext_flash_ready == 0U) || (data == 0) || (length == 0U) || (length > GD25Q128E_PAGE_SIZE)) {
        return 0U;
    }

    if ((address >= GD25Q128E_SIZE_BYTES) || ((GD25Q128E_SIZE_BYTES - address) < length)) {
        return 0U;
    }

    page_remaining = GD25Q128E_PAGE_SIZE - (address & (GD25Q128E_PAGE_SIZE - 1UL));
    if (length > page_remaining) {
        return 0U;
    }

    if (ExtFlash_WriteEnable() == 0U) {
        return 0U;
    }

    ExtFlash_Select();
    ExtFlash_WriteByte(GD25Q128E_CMD_PAGE_PROGRAM);
    ExtFlash_WriteByte((uint8_t)(address >> 16));
    ExtFlash_WriteByte((uint8_t)(address >> 8));
    ExtFlash_WriteByte((uint8_t)address);
    for (i = 0U; i < length; i++) {
        ExtFlash_WriteByte(data[i]);
    }
    ExtFlash_Deselect();

    return ExtFlash_WaitReady(100U);
}
