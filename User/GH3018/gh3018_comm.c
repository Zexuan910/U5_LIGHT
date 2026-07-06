#include "gh3018_comm.h"

#include "hbd_ctrl.h"
#include "main.h"

#include <string.h>

#define GH3018_I2C_ADDR_7BIT 0x14U
#define GH3018_I2C_ADDR_HAL  (GH3018_I2C_ADDR_7BIT << 1)
#define GH3018_I2C_TIMEOUT_MS 100U

extern I2C_HandleTypeDef hi2c1;

Gh3018CommSnapshot g_gh3018_comm_snapshot;

static uint16_t Gh3018Comm_DeviceIdToHalAddr(uint8_t deviceId)
{
    if ((deviceId >= 0x28U) && (deviceId <= 0x2FU)) {
        return (uint16_t)(deviceId & 0xFEU);
    }

    if ((deviceId >= 0x14U) && (deviceId <= 0x17U)) {
        return (uint16_t)(deviceId << 1);
    }

    return GH3018_I2C_ADDR_HAL;
}

static void Gh3018Comm_UpdatePins(void)
{
    g_gh3018_comm_snapshot.hbdOnLevel = (uint8_t)HAL_GPIO_ReadPin(GH3018_HBD_ON_GPIO_Port, GH3018_HBD_ON_Pin);
    g_gh3018_comm_snapshot.intLevel = (uint8_t)HAL_GPIO_ReadPin(GH3018_INT_GPIO_Port, GH3018_INT_Pin);
}

static void Gh3018Comm_SaveHalStatus(HAL_StatusTypeDef status)
{
    g_gh3018_comm_snapshot.lastHalStatus = status;
    g_gh3018_comm_snapshot.lastHalError = HAL_I2C_GetError(&hi2c1);
    Gh3018Comm_UpdatePins();
}

static void Gh3018Comm_DelayUs(uint16_t usec)
{
    if (usec >= 1000U) {
        HAL_Delay((uint32_t)((usec + 999U) / 1000U));
        return;
    }

    uint32_t cycles = (HAL_RCC_GetHCLKFreq() / 1000000U) * (uint32_t)usec;
    cycles = (cycles / 6U) + 1U;
    while (cycles-- > 0U) {
        __NOP();
    }
}

static void Gh3018Comm_HardwareReset(void)
{
    HAL_GPIO_WritePin(GH3018_RSTN_GPIO_Port, GH3018_RSTN_Pin, GPIO_PIN_SET);
    HAL_Delay(5U);
    HAL_GPIO_WritePin(GH3018_RSTN_GPIO_Port, GH3018_RSTN_Pin, GPIO_PIN_RESET);
    HAL_Delay(2U);
    HAL_GPIO_WritePin(GH3018_RSTN_GPIO_Port, GH3018_RSTN_Pin, GPIO_PIN_SET);
    HAL_Delay(10U);
    Gh3018Comm_UpdatePins();
}

static uint8_t Gh3018Comm_I2cWrite(uint8_t deviceId, const uint8_t writeBytes[], uint16_t writeLen)
{
    uint16_t halAddr = Gh3018Comm_DeviceIdToHalAddr(deviceId);
    g_gh3018_comm_snapshot.lastWriteDeviceId = deviceId;
    g_gh3018_comm_snapshot.i2cWriteCount++;

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1,
                                                       halAddr,
                                                       (uint8_t *)(uintptr_t)writeBytes,
                                                       writeLen,
                                                       GH3018_I2C_TIMEOUT_MS);
    Gh3018Comm_SaveHalStatus(status);
    return (status == HAL_OK) ? 0U : 1U;
}

static uint8_t Gh3018Comm_I2cRead(uint8_t deviceId,
                                  const uint8_t commandBytes[],
                                  uint16_t commandLen,
                                  uint8_t readBytes[],
                                  uint16_t maxReadLen)
{
    uint16_t halAddr = Gh3018Comm_DeviceIdToHalAddr(deviceId);
    g_gh3018_comm_snapshot.lastReadDeviceId = deviceId;
    g_gh3018_comm_snapshot.i2cReadCount++;

    if ((commandBytes != NULL) && (commandLen > 0U)) {
        HAL_StatusTypeDef txStatus = HAL_I2C_Master_Transmit(&hi2c1,
                                                             halAddr,
                                                             (uint8_t *)(uintptr_t)commandBytes,
                                                             commandLen,
                                                             GH3018_I2C_TIMEOUT_MS);
        Gh3018Comm_SaveHalStatus(txStatus);
        if (txStatus != HAL_OK) {
            return 1U;
        }
        Gh3018Comm_DelayUs(20U);
    }

    HAL_StatusTypeDef rxStatus = HAL_I2C_Master_Receive(&hi2c1,
                                                        halAddr,
                                                        readBytes,
                                                        maxReadLen,
                                                        GH3018_I2C_TIMEOUT_MS);
    Gh3018Comm_SaveHalStatus(rxStatus);
    return (rxStatus == HAL_OK) ? 0U : 1U;
}

const Gh3018CommSnapshot *Gh3018Comm_RunSelfTest(void)
{
    memset(&g_gh3018_comm_snapshot, 0, sizeof(g_gh3018_comm_snapshot));
    g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_IDLE;
    g_gh3018_comm_snapshot.hbdSetI2cRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_comm_snapshot.hbdConfirmRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_comm_snapshot.hbdChipIdRet = HBD_RET_GENERIC_ERROR;

    Gh3018Comm_HardwareReset();

    HBD_SetDelayUsCallback(Gh3018Comm_DelayUs);
    g_gh3018_comm_snapshot.hbdSetI2cRet = HBD_SetI2cRW(HBD_I2C_ID_SEL_1L0L,
                                                       Gh3018Comm_I2cWrite,
                                                       Gh3018Comm_I2cRead);
    if (g_gh3018_comm_snapshot.hbdSetI2cRet != HBD_RET_OK) {
        g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_HBD_REGISTER_FAIL;
        return &g_gh3018_comm_snapshot;
    }

    g_gh3018_comm_snapshot.deviceReadyStatus = HAL_I2C_IsDeviceReady(&hi2c1,
                                                                     GH3018_I2C_ADDR_HAL,
                                                                     3U,
                                                                     GH3018_I2C_TIMEOUT_MS);
    Gh3018Comm_SaveHalStatus(g_gh3018_comm_snapshot.deviceReadyStatus);
    if (g_gh3018_comm_snapshot.deviceReadyStatus != HAL_OK) {
        g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_I2C_FAIL;
        return &g_gh3018_comm_snapshot;
    }

    g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_I2C_READY;
    g_gh3018_comm_snapshot.hbdConfirmRet = HBD_CommunicationInterfaceConfirm();
    if (g_gh3018_comm_snapshot.hbdConfirmRet != HBD_RET_OK) {
        g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_HBD_CONFIRM_FAIL;
        return &g_gh3018_comm_snapshot;
    }

    g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_HBD_CONFIRM_OK;
    g_gh3018_comm_snapshot.chipIdLen = (uint8_t)sizeof(g_gh3018_comm_snapshot.chipId);
    g_gh3018_comm_snapshot.hbdChipIdRet = HBD_GetChipId(g_gh3018_comm_snapshot.chipId,
                                                        &g_gh3018_comm_snapshot.chipIdLen);
    if (g_gh3018_comm_snapshot.hbdChipIdRet != HBD_RET_OK) {
        g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_CHIP_ID_FAIL;
        return &g_gh3018_comm_snapshot;
    }

    g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_CHIP_ID_OK;
    return &g_gh3018_comm_snapshot;
}

const Gh3018CommSnapshot *Gh3018Comm_GetSnapshot(void)
{
    Gh3018Comm_UpdatePins();
    return &g_gh3018_comm_snapshot;
}