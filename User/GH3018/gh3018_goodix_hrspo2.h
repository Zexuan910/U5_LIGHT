#ifndef GH3018_GOODIX_HRSPO2_H
#define GH3018_GOODIX_HRSPO2_H

#include "gh3018_comm.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GH3018_GOODIX_HRSPO2_STATUS_IDLE = 0,
    GH3018_GOODIX_HRSPO2_STATUS_COMM_FAIL,
    GH3018_GOODIX_HRSPO2_STATUS_SIMPLE_INIT_FAIL,
    GH3018_GOODIX_HRSPO2_STATUS_READY,
    GH3018_GOODIX_HRSPO2_STATUS_START_FAIL,
    GH3018_GOODIX_HRSPO2_STATUS_LED_SET_FAIL,
    GH3018_GOODIX_HRSPO2_STATUS_WAITING_DATA,
    GH3018_GOODIX_HRSPO2_STATUS_RUNNING,
    GH3018_GOODIX_HRSPO2_STATUS_RESULT_REFRESHED,
    GH3018_GOODIX_HRSPO2_STATUS_CALC_FAIL,
    GH3018_GOODIX_HRSPO2_STATUS_GREEN_RAW_FALLBACK,
    GH3018_GOODIX_HRSPO2_STATUS_STOPPED,
    GH3018_GOODIX_HRSPO2_STATUS_STOP_FAIL
} Gh3018GoodixHrSpo2Status;

typedef enum {
    GH3018_GOODIX_HRSPO2_WEAR_REASON_NONE = 0,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_RAW_RECENT,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_AC_RANGE,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_PEAK_RECENT,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_INTERVAL_RECENT,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_HR_VALID,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_RAW_TIMEOUT,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_LOW_SCORE,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_DC_DELTA,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_MOTION_FREEZE,
    GH3018_GOODIX_HRSPO2_WEAR_REASON_MANUAL
} Gh3018GoodixHrSpo2WearReason;

typedef enum {
    GH3018_GOODIX_HRSPO2_BPM_GATE_NONE = 0,
    GH3018_GOODIX_HRSPO2_BPM_GATE_PPG_INVALID,
    GH3018_GOODIX_HRSPO2_BPM_GATE_WEAR_OFF,
    GH3018_GOODIX_HRSPO2_BPM_GATE_LOW_WEAR_SCORE,
    GH3018_GOODIX_HRSPO2_BPM_GATE_INTERVAL_STALE,
    GH3018_GOODIX_HRSPO2_BPM_GATE_RAW_TIMEOUT,
    GH3018_GOODIX_HRSPO2_BPM_GATE_MOTION_FREEZE
} Gh3018GoodixHrSpo2BpmGateReason;

typedef struct {
    Gh3018GoodixHrSpo2Status status;
    Gh3018CommStatus commStatus;
    Gh3018SoftI2cStatus lastSoftI2cStatus;
    int8_t hbdSimpleInitRet;
    int8_t hbdHrSpo2StartRet;
    int8_t hbdSetCurrentRet;
    int8_t hbdGetCurrentRet;
    int8_t hbdStopRet;
    int8_t hbdCalcRet;
    uint8_t measurementActive;
    uint8_t rstnLevel;
    uint8_t hbdOnLevel;
    uint8_t intLevel;
    uint8_t intStatus;
    uint8_t ledLogicMap;
    uint8_t channel0CurrentStep;
    uint8_t channel1CurrentStep;
    int16_t channel0CurrentX10;
    int16_t channel1CurrentX10;
    int16_t led0CurrentX10;
    int16_t led1CurrentX10;
    uint8_t heartRate;
    uint8_t heartRateConfidence;
    uint8_t heartRateValid;
    uint8_t ppgHeartRate;
    uint8_t ppgHeartRateConfidence;
    uint8_t ppgHeartRateValid;
    uint8_t ppgSignalQuality;
    uint8_t ppgAutoCorrBpm;
    uint8_t ppgAutoCorrScore;
    uint8_t ppgAutoCorrValid;
    uint8_t ppgAutoCorrBestScore;
    uint8_t ppgAutoCorrSecondScore;
    uint8_t ppgAutoCorrRejectReason;
    uint8_t ppgMotionFreeze;
    uint8_t ppgDisplayedBpm;
    uint8_t ppgPendingBpm;
    uint16_t ppgAutoCorrLag;
    uint16_t ppgAutoCorrBestLag;
    uint16_t ppgAutoCorrSecondLag;
    int32_t ppgDcDrift1s;
    uint8_t ppgWearingState;
    uint8_t ppgWearScore;
    uint8_t ppgWearReason;
    uint32_t ppgSampleCount;
    uint32_t ppgPeakCount;
    uint32_t ppgAcRange;
    uint16_t ppgLastIntervalMs;
    uint32_t ppgCandidatePeakCount;
    uint32_t ppgIntervalAcceptedCount;
    uint32_t ppgIntervalTooShortCount;
    uint32_t ppgIntervalTooLongCount;
    uint32_t ppgIntervalRejectedOutlierCount;
    uint16_t ppgLastRejectedIntervalMs;
    uint32_t ppgPeakThreshold;
    uint32_t ppgRawAcceptedCount;
    uint32_t ppgRawRejectedZeroCount;
    uint32_t ppgRawRejectedJumpCount;
    uint32_t ppgRawResyncCount;
    int32_t ppgLastAcceptedRaw;
    uint32_t ppgRawJumpThreshold;
    int32_t ppgWearEmptyDc;
    int32_t ppgWearDcDelta;
    uint32_t ppgWearDcThreshold;
    uint32_t ppgWearStableMs;
    uint32_t ppgRawAgeMs;
    uint32_t ppgPeakAgeMs;
    uint32_t ppgCandidateAgeMs;
    uint32_t ppgIntervalAgeMs;
    uint32_t ppgIntervalExpiredCount;
    uint32_t ppgPendingPeakExpiredCount;
    uint8_t ppgBpmGateReason;
    uint8_t goodixHeartRate;
    uint8_t goodixHeartRateConfidence;
    uint8_t goodixHeartRateValid;
    uint8_t goodixWearingState;
    uint8_t spo2;
    uint8_t spo2Confidence;
    uint8_t spo2Valid;
    uint8_t spo2Simulated;
    uint8_t wearingState;
    uint8_t hrvCount;
    uint8_t hrvConfidence;
    uint16_t spo2RValue;
    uint16_t hrvRR[4];
    int32_t spo2ValidLevel;
    int32_t spo2InvalidFlag;
    uint32_t spo2SimSwitchCount;
    uint32_t spo2SimNextSwitchMs;
    uint16_t rawDataLen;
    uint8_t rawFifoCount;
    int32_t rawPpg0;
    int32_t rawPpg1;
    uint32_t rawMaxPpg0;
    uint32_t rawMaxPpg1;
    uint32_t rawProbeCount;
    uint32_t rawNonzeroCount;
    uint32_t rawChangeCount;
    uint32_t rawEmptyCount;
    uint32_t rawBufferFullCount;
    uint32_t sessionId;
    uint32_t startCount;
    uint32_t stopCount;
    uint32_t pollCount;
    uint32_t calcCount;
    uint32_t resultRefreshCount;
    uint32_t noDataCount;
    uint32_t sessionPollCount;
    uint32_t sessionCalcCount;
    uint32_t sessionResultRefreshCount;
    uint32_t sessionNoDataCount;
    uint32_t intChipResetCount;
    uint32_t intNewDataCount;
    uint32_t intFifoWatermarkCount;
    uint32_t intFifoFullCount;
    uint32_t intWearCount;
    uint32_t intUnwearCount;
    uint32_t intInvalidCount;
    uint32_t lastUpdateTick;
    uint32_t lastResultTick;
    uint32_t i2cWriteCount;
    uint32_t i2cReadCount;
    uint32_t busRecoveryCount;
    uint32_t addressNackCount;
    uint32_t dataNackCount;
    uint32_t sclTimeoutCount;
} Gh3018GoodixHrSpo2Snapshot;

extern Gh3018GoodixHrSpo2Snapshot g_gh3018_goodix_hrspo2_snapshot;

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_Init(void);
const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_Start(void);
const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_Stop(void);
const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_Poll(void);
const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_BeginWearCheck(void);
const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_ResetPpgBpm(void);
const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_SetManualWear(
    uint8_t enabled);
const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_GetSnapshot(void);
const char *Gh3018GoodixHrSpo2_StatusName(Gh3018GoodixHrSpo2Status status);

#ifdef __cplusplus
}
#endif

#endif
