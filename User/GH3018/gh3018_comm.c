#include "gh3018_comm.h"

#include "hbd_ctrl.h"
#include "main.h"

#include <string.h>

#define GH3018_I2C_ADDR_7BIT 0x14U

Gh3018CommSnapshot g_gh3018_comm_snapshot;

static uint8_t Gh3018Comm_DeviceIdTo7BitAddr(uint8_t deviceId)
{
    if ((deviceId >= 0x28U) && (deviceId <= 0x2FU)) {
        return (uint8_t)((deviceId & 0xFEU) >> 1);
    }

    if ((deviceId >= 0x14U) && (deviceId <= 0x17U)) {
        return deviceId;
    }

    return GH3018_I2C_ADDR_7BIT;
}

static void Gh3018Comm_UpdatePins(void)
{
    g_gh3018_comm_snapshot.hbdOnLevel = (uint8_t)HAL_GPIO_ReadPin(GH3018_HBD_ON_GPIO_Port, GH3018_HBD_ON_Pin);
    g_gh3018_comm_snapshot.intLevel = (uint8_t)HAL_GPIO_ReadPin(GH3018_INT_GPIO_Port, GH3018_INT_Pin);
}

static void Gh3018Comm_CopySoftI2cStats(void)
{
    const Gh3018SoftI2cStats *stats = Gh3018SoftI2c_GetStats();
    g_gh3018_comm_snapshot.busRecoveryCount = stats->busRecoveryCount;
    g_gh3018_comm_snapshot.addressNackCount = stats->addressNackCount;
    g_gh3018_comm_snapshot.dataNackCount = stats->dataNackCount;
    g_gh3018_comm_snapshot.sclTimeoutCount = stats->sclTimeoutCount;
}

static void Gh3018Comm_SaveSoftI2cStatus(Gh3018SoftI2cStatus status)
{
    g_gh3018_comm_snapshot.lastSoftI2cStatus = status;
    Gh3018Comm_CopySoftI2cStats();
    Gh3018Comm_UpdatePins();
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
    uint8_t address7bit = Gh3018Comm_DeviceIdTo7BitAddr(deviceId);
    g_gh3018_comm_snapshot.lastWriteDeviceId = deviceId;
    g_gh3018_comm_snapshot.i2cWriteCount++;

    Gh3018SoftI2cStatus status = Gh3018SoftI2c_Write(address7bit, writeBytes, writeLen);
    Gh3018Comm_SaveSoftI2cStatus(status);
    return (status == GH3018_SOFT_I2C_OK) ? 0U : 1U;
}

static uint8_t Gh3018Comm_I2cRead(uint8_t deviceId,
                                  const uint8_t commandBytes[],
                                  uint16_t commandLen,
                                  uint8_t readBytes[],
                                  uint16_t maxReadLen)
{
    uint8_t address7bit = Gh3018Comm_DeviceIdTo7BitAddr(deviceId);
    g_gh3018_comm_snapshot.lastReadDeviceId = deviceId;
    g_gh3018_comm_snapshot.i2cReadCount++;

    if ((commandBytes != NULL) && (commandLen > 0U)) {
        Gh3018SoftI2cStatus txStatus = Gh3018SoftI2c_Write(address7bit,
                                                           commandBytes,
                                                           commandLen);
        Gh3018Comm_SaveSoftI2cStatus(txStatus);
        if (txStatus != GH3018_SOFT_I2C_OK) {
            return 1U;
        }
        Gh3018SoftI2c_DelayUs(20U);
    }

    Gh3018SoftI2cStatus rxStatus = Gh3018SoftI2c_Read(address7bit, readBytes, maxReadLen);
    Gh3018Comm_SaveSoftI2cStatus(rxStatus);
    return (rxStatus == GH3018_SOFT_I2C_OK) ? 0U : 1U;
}

const Gh3018CommSnapshot *Gh3018Comm_RunSelfTest(void)
{
    memset(&g_gh3018_comm_snapshot, 0, sizeof(g_gh3018_comm_snapshot));
    g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_IDLE;
    g_gh3018_comm_snapshot.softI2cInitStatus = GH3018_SOFT_I2C_INVALID_ARG;
    g_gh3018_comm_snapshot.deviceReadyStatus = GH3018_SOFT_I2C_INVALID_ARG;
    g_gh3018_comm_snapshot.lastSoftI2cStatus = GH3018_SOFT_I2C_INVALID_ARG;
    g_gh3018_comm_snapshot.hbdSetI2cRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_comm_snapshot.hbdConfirmRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_comm_snapshot.hbdChipIdRet = HBD_RET_GENERIC_ERROR;

    Gh3018Comm_HardwareReset();

    g_gh3018_comm_snapshot.softI2cInitStatus = Gh3018SoftI2c_Init();
    Gh3018Comm_SaveSoftI2cStatus(g_gh3018_comm_snapshot.softI2cInitStatus);
    if (g_gh3018_comm_snapshot.softI2cInitStatus != GH3018_SOFT_I2C_OK) {
        g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_I2C_FAIL;
        return &g_gh3018_comm_snapshot;
    }

    HBD_SetDelayUsCallback(Gh3018SoftI2c_DelayUs);
    g_gh3018_comm_snapshot.hbdSetI2cRet = HBD_SetI2cRW(HBD_I2C_ID_SEL_1L0L,
                                                       Gh3018Comm_I2cWrite,
                                                       Gh3018Comm_I2cRead);
    if (g_gh3018_comm_snapshot.hbdSetI2cRet != HBD_RET_OK) {
        g_gh3018_comm_snapshot.status = GH3018_COMM_STATUS_HBD_REGISTER_FAIL;
        return &g_gh3018_comm_snapshot;
    }

    g_gh3018_comm_snapshot.deviceReadyStatus = Gh3018SoftI2c_Probe(GH3018_I2C_ADDR_7BIT);
    Gh3018Comm_SaveSoftI2cStatus(g_gh3018_comm_snapshot.deviceReadyStatus);
    if (g_gh3018_comm_snapshot.deviceReadyStatus != GH3018_SOFT_I2C_OK) {
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
    Gh3018Comm_CopySoftI2cStats();
    Gh3018Comm_UpdatePins();
    return &g_gh3018_comm_snapshot;
}
