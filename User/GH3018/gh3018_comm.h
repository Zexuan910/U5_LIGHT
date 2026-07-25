#ifndef GH3018_COMM_H
#define GH3018_COMM_H

#include "gh3018_soft_i2c.h"
#include <stdint.h>

#define GH3018_COMM_CHIP_ID_MAX_LEN 16U

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GH3018_COMM_STATUS_IDLE = 0,
    GH3018_COMM_STATUS_I2C_READY,
    GH3018_COMM_STATUS_HBD_CONFIRM_OK,
    GH3018_COMM_STATUS_CHIP_ID_OK,
    GH3018_COMM_STATUS_I2C_FAIL,
    GH3018_COMM_STATUS_HBD_REGISTER_FAIL,
    GH3018_COMM_STATUS_HBD_CONFIRM_FAIL,
    GH3018_COMM_STATUS_CHIP_ID_FAIL
} Gh3018CommStatus;

typedef struct {
    Gh3018CommStatus status;
    Gh3018SoftI2cStatus softI2cInitStatus;
    Gh3018SoftI2cStatus deviceReadyStatus;
    Gh3018SoftI2cStatus lastSoftI2cStatus;
    int8_t hbdSetI2cRet;
    int8_t hbdConfirmRet;
    int8_t hbdChipIdRet;
    uint8_t lastWriteDeviceId;
    uint8_t lastReadDeviceId;
    uint8_t chipId[GH3018_COMM_CHIP_ID_MAX_LEN];
    uint8_t chipIdLen;
    uint8_t hbdOnLevel;
    uint8_t intLevel;
    uint32_t i2cWriteCount;
    uint32_t i2cReadCount;
    uint32_t busRecoveryCount;
    uint32_t addressNackCount;
    uint32_t dataNackCount;
    uint32_t sclTimeoutCount;
} Gh3018CommSnapshot;

extern Gh3018CommSnapshot g_gh3018_comm_snapshot;

const Gh3018CommSnapshot *Gh3018Comm_RunSelfTest(void);
const Gh3018CommSnapshot *Gh3018Comm_GetSnapshot(void);

#ifdef __cplusplus
}
#endif

#endif
