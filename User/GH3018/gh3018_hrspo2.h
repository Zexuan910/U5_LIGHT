#ifndef GH3018_HRSPO2_H
#define GH3018_HRSPO2_H

#include "gh3018_comm.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GH3018_HRSPO2_STATUS_IDLE = 0,
    GH3018_HRSPO2_STATUS_COMM_FAIL,
    GH3018_HRSPO2_STATUS_SIMPLE_INIT_FAIL,
    GH3018_HRSPO2_STATUS_START_FAIL,
    GH3018_HRSPO2_STATUS_WAITING_DATA,
    GH3018_HRSPO2_STATUS_RUNNING,
    GH3018_HRSPO2_STATUS_RESULT_REFRESHED,
    GH3018_HRSPO2_STATUS_CALC_FAIL
} Gh3018HrSpo2Status;

typedef struct {
    Gh3018HrSpo2Status status;
    Gh3018CommStatus commStatus;
    int8_t hbdSimpleInitRet;
    int8_t hbdHrSpo2StartRet;
    int8_t hbdCalcRet;
    uint8_t intStatus;
    uint8_t hbdOnLevel;
    uint8_t intLevel;
    uint8_t heartRate;
    uint8_t heartRateConfidence;
    uint8_t spo2;
    uint8_t spo2Confidence;
    uint8_t wearingState;
    uint8_t hrvCount;
    uint8_t hrvConfidence;
    uint16_t spo2RValue;
    uint16_t hrvRR[4];
    int32_t spo2InvalidFlag;
    uint16_t rawDataLen;
    uint32_t pollCount;
    uint32_t calcCount;
    uint32_t resultRefreshCount;
    uint32_t noDataCount;
    uint32_t lastUpdateTick;
    uint32_t i2cWriteCount;
    uint32_t i2cReadCount;
} Gh3018HrSpo2Snapshot;

extern Gh3018HrSpo2Snapshot g_gh3018_hrspo2_snapshot;

const Gh3018HrSpo2Snapshot *Gh3018HrSpo2_Init(void);
const Gh3018HrSpo2Snapshot *Gh3018HrSpo2_Poll(void);
const Gh3018HrSpo2Snapshot *Gh3018HrSpo2_GetSnapshot(void);
const char *Gh3018HrSpo2_StatusName(Gh3018HrSpo2Status status);

#ifdef __cplusplus
}
#endif

#endif
