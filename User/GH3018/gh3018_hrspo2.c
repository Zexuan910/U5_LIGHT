#include "gh3018_hrspo2.h"

#include "hbd_ctrl.h"
#include "main.h"

#include <string.h>

#define GH3018_HRSPO2_RAW_FRAME_CAPACITY 16U

Gh3018HrSpo2Snapshot g_gh3018_hrspo2_snapshot;

static GS32 s_rawData[GH3018_HRSPO2_RAW_FRAME_CAPACITY][6];
static ST_SPO2_RES s_spo2Result;

static void Gh3018HrSpo2_UpdatePins(void)
{
    g_gh3018_hrspo2_snapshot.hbdOnLevel =
        (uint8_t)HAL_GPIO_ReadPin(GH3018_HBD_ON_GPIO_Port, GH3018_HBD_ON_Pin);
    g_gh3018_hrspo2_snapshot.intLevel =
        (uint8_t)HAL_GPIO_ReadPin(GH3018_INT_GPIO_Port, GH3018_INT_Pin);
}

static void Gh3018HrSpo2_CopyCommSnapshot(const Gh3018CommSnapshot *comm)
{
    g_gh3018_hrspo2_snapshot.commStatus = comm->status;
    g_gh3018_hrspo2_snapshot.i2cWriteCount = comm->i2cWriteCount;
    g_gh3018_hrspo2_snapshot.i2cReadCount = comm->i2cReadCount;
}

static void Gh3018HrSpo2_CopyResult(void)
{
    g_gh3018_hrspo2_snapshot.heartRate = s_spo2Result.uchHbValue;
    g_gh3018_hrspo2_snapshot.heartRateConfidence = s_spo2Result.uchHbConfidentLvl;
    g_gh3018_hrspo2_snapshot.spo2 = s_spo2Result.uchSpo2;
    g_gh3018_hrspo2_snapshot.spo2Confidence = s_spo2Result.uchSpo2Confidence;
    g_gh3018_hrspo2_snapshot.wearingState = s_spo2Result.uchWearingState;
    g_gh3018_hrspo2_snapshot.hrvCount = s_spo2Result.uchHrvcnt;
    g_gh3018_hrspo2_snapshot.hrvConfidence = s_spo2Result.uchHrvConfidentLvl;
    g_gh3018_hrspo2_snapshot.spo2RValue = s_spo2Result.usSpo2RVal;
    g_gh3018_hrspo2_snapshot.spo2InvalidFlag = s_spo2Result.uliSpo2InvlaidFlag;
    for (uint8_t i = 0U; i < 4U; ++i) {
        g_gh3018_hrspo2_snapshot.hrvRR[i] = s_spo2Result.usHrvRRVal[i];
    }
}

static uint8_t Gh3018HrSpo2_IntHasData(uint8_t intStatus)
{
    return (intStatus == INT_STATUS_NEW_DATA) ||
           (intStatus == INT_STATUS_FIFO_WATERMARK) ||
           (intStatus == INT_STATUS_FIFO_FULL) ||
           (intStatus == INT_STATUS_WEAR_DETECTED) ||
           (intStatus == INT_STATUS_UNWEAR_DETECTED);
}

const Gh3018HrSpo2Snapshot *Gh3018HrSpo2_Init(void)
{
    memset(&g_gh3018_hrspo2_snapshot, 0, sizeof(g_gh3018_hrspo2_snapshot));
    memset(&s_spo2Result, 0, sizeof(s_spo2Result));
    g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_IDLE;
    g_gh3018_hrspo2_snapshot.hbdSimpleInitRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_hrspo2_snapshot.hbdHrSpo2StartRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_hrspo2_snapshot.hbdCalcRet = HBD_RET_GENERIC_ERROR;
    Gh3018HrSpo2_UpdatePins();

    const Gh3018CommSnapshot *comm = Gh3018Comm_RunSelfTest();
    Gh3018HrSpo2_CopyCommSnapshot(comm);
    Gh3018HrSpo2_UpdatePins();
    if (comm->status != GH3018_COMM_STATUS_CHIP_ID_OK) {
        g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_COMM_FAIL;
        return &g_gh3018_hrspo2_snapshot;
    }

    HBD_INIT_CONFIG_DEFAULT_DEF(initConfig);
    g_gh3018_hrspo2_snapshot.hbdSimpleInitRet = HBD_SimpleInit(&initConfig);
    Gh3018HrSpo2_UpdatePins();
    if (g_gh3018_hrspo2_snapshot.hbdSimpleInitRet != HBD_RET_OK) {
        g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_SIMPLE_INIT_FAIL;
        return &g_gh3018_hrspo2_snapshot;
    }

    g_gh3018_hrspo2_snapshot.hbdHrSpo2StartRet = HBD_HrSpO2DetectStart();
    Gh3018HrSpo2_UpdatePins();
    if (g_gh3018_hrspo2_snapshot.hbdHrSpo2StartRet != HBD_RET_OK) {
        g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_START_FAIL;
        return &g_gh3018_hrspo2_snapshot;
    }

    g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_WAITING_DATA;
    g_gh3018_hrspo2_snapshot.lastUpdateTick = HAL_GetTick();
    return &g_gh3018_hrspo2_snapshot;
}

const Gh3018HrSpo2Snapshot *Gh3018HrSpo2_Poll(void)
{
    if ((g_gh3018_hrspo2_snapshot.status == GH3018_HRSPO2_STATUS_COMM_FAIL) ||
        (g_gh3018_hrspo2_snapshot.status == GH3018_HRSPO2_STATUS_SIMPLE_INIT_FAIL) ||
        (g_gh3018_hrspo2_snapshot.status == GH3018_HRSPO2_STATUS_START_FAIL)) {
        Gh3018HrSpo2_UpdatePins();
        return &g_gh3018_hrspo2_snapshot;
    }

    g_gh3018_hrspo2_snapshot.pollCount++;
    g_gh3018_hrspo2_snapshot.intStatus = HBD_GetIntStatus();
    Gh3018HrSpo2_UpdatePins();

    if (!Gh3018HrSpo2_IntHasData(g_gh3018_hrspo2_snapshot.intStatus)) {
        g_gh3018_hrspo2_snapshot.noDataCount++;
        if (g_gh3018_hrspo2_snapshot.status == GH3018_HRSPO2_STATUS_WAITING_DATA) {
            return &g_gh3018_hrspo2_snapshot;
        }
        g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_RUNNING;
        return &g_gh3018_hrspo2_snapshot;
    }

    memset(s_rawData, 0, sizeof(s_rawData));
    memset(&s_spo2Result, 0, sizeof(s_spo2Result));
    g_gh3018_hrspo2_snapshot.rawDataLen = GH3018_HRSPO2_RAW_FRAME_CAPACITY;
    g_gh3018_hrspo2_snapshot.hbdCalcRet = HBD_HrSpo2CalculateByFifoIntEx(
        NULL,
        0U,
        HBD_GSENSOR_SENSITIVITY_1024_COUNTS_PER_G,
        s_rawData,
        &g_gh3018_hrspo2_snapshot.rawDataLen,
        &s_spo2Result);
    g_gh3018_hrspo2_snapshot.calcCount++;
    g_gh3018_hrspo2_snapshot.lastUpdateTick = HAL_GetTick();

    if (g_gh3018_hrspo2_snapshot.hbdCalcRet < 0) {
        g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_CALC_FAIL;
        return &g_gh3018_hrspo2_snapshot;
    }

    if (g_gh3018_hrspo2_snapshot.hbdCalcRet > 0) {
        Gh3018HrSpo2_CopyResult();
        g_gh3018_hrspo2_snapshot.resultRefreshCount++;
        g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_RESULT_REFRESHED;
        return &g_gh3018_hrspo2_snapshot;
    }

    g_gh3018_hrspo2_snapshot.status = GH3018_HRSPO2_STATUS_RUNNING;
    return &g_gh3018_hrspo2_snapshot;
}

const Gh3018HrSpo2Snapshot *Gh3018HrSpo2_GetSnapshot(void)
{
    Gh3018HrSpo2_UpdatePins();
    return &g_gh3018_hrspo2_snapshot;
}

const char *Gh3018HrSpo2_StatusName(Gh3018HrSpo2Status status)
{
    switch (status) {
    case GH3018_HRSPO2_STATUS_IDLE:
        return "IDLE";
    case GH3018_HRSPO2_STATUS_COMM_FAIL:
        return "COMM_FAIL";
    case GH3018_HRSPO2_STATUS_SIMPLE_INIT_FAIL:
        return "SIMPLE_INIT_FAIL";
    case GH3018_HRSPO2_STATUS_START_FAIL:
        return "START_FAIL";
    case GH3018_HRSPO2_STATUS_WAITING_DATA:
        return "WAITING_DATA";
    case GH3018_HRSPO2_STATUS_RUNNING:
        return "RUNNING";
    case GH3018_HRSPO2_STATUS_RESULT_REFRESHED:
        return "RESULT_REFRESHED";
    case GH3018_HRSPO2_STATUS_CALC_FAIL:
        return "CALC_FAIL";
    default:
        return "UNKNOWN";
    }
}
