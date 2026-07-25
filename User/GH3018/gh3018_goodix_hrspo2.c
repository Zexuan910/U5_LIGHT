#include "gh3018_goodix_hrspo2.h"

#include "gh3018_green_ppg_bpm.h"
#include "hbd_ctrl.h"
#include "main.h"

#include <string.h>

#define GH3018_GOODIX_HRSPO2_RAW_FRAME_CAPACITY 16U
#define GH3018_GOODIX_HRSPO2_GREEN_CURRENT_STEP 0xFAU
#define GH3018_GOODIX_HRSPO2_RED_CURRENT_STEP 0x00U
#define GH3018_GOODIX_HRSPO2_GREEN_CURRENT_MA 100.0f
#define GH3018_GOODIX_HRSPO2_RED_CURRENT_MA 0.0f
#define GH3018_GOODIX_HRSPO2_GREEN_SAMPLE_RATE_HZ 25U
#define GH3018_GOODIX_HRSPO2_GREEN_FIFO_ENABLE 0U
#define GH3018_GOODIX_HRSPO2_GREEN_FIFO_THR 0U
#define GH3018_GOODIX_HRSPO2_NO_DATA_FALLBACK_THRESHOLD 20U
#define GH3018_GOODIX_HRSPO2_GREEN_NO_DATA_RESTART_THRESHOLD 5U
#define GH3018_GOODIX_HRSPO2_PPG_RAW_JUMP_MIN_THRESHOLD 50000U
#define GH3018_GOODIX_HRSPO2_PPG_RAW_JUMP_DIVISOR 20U
#define GH3018_GOODIX_HRSPO2_PPG_RAW_RESYNC_REJECTS 5U
#define GH3018_GOODIX_HRSPO2_WEAR_RAW_RECENT_MS 1200U
#define GH3018_GOODIX_HRSPO2_WEAR_RAW_TIMEOUT_MS 1500U
#define GH3018_GOODIX_HRSPO2_WEAR_PEAK_RECENT_MS 2500U
#define GH3018_GOODIX_HRSPO2_WEAR_INTERVAL_RECENT_MS 4000U
#define GH3018_GOODIX_HRSPO2_BPM_INTERVAL_FRESH_MS 1800U
#define GH3018_GOODIX_HRSPO2_BPM_INTERVAL_CLEAR_MS 2500U
#define GH3018_GOODIX_HRSPO2_WEAR_ON_SCORE 80U
#define GH3018_GOODIX_HRSPO2_WEAR_OFF_SCORE 40U
#define GH3018_GOODIX_HRSPO2_WEAR_ON_DEBOUNCE_MS 1000U
#define GH3018_GOODIX_HRSPO2_WEAR_OFF_DEBOUNCE_MS 1500U
#define GH3018_GOODIX_HRSPO2_WEAR_MIN_RAW_ACCEPTED 25U
#define GH3018_GOODIX_HRSPO2_WEAR_DC_FILTER_DIV 32
#define GH3018_GOODIX_HRSPO2_WEAR_EMPTY_DC_FILTER_DIV 128
#define GH3018_GOODIX_HRSPO2_WEAR_ON_DC_THRESHOLD_MIN 8000U
#define GH3018_GOODIX_HRSPO2_WEAR_OFF_DC_THRESHOLD_MIN 4000U
#define GH3018_GOODIX_HRSPO2_WEAR_ON_DC_THRESHOLD_DIV 1000U
#define GH3018_GOODIX_HRSPO2_WEAR_OFF_DC_THRESHOLD_DIV 2000U
#define GH3018_GOODIX_HRSPO2_BPM_MIN 40U
#define GH3018_GOODIX_HRSPO2_BPM_MAX 150U
#define GH3018_GOODIX_HRSPO2_SPO2_SIM_SWITCH_MS 2000U
#define GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID 0xFFFFFFFFU

Gh3018GoodixHrSpo2Snapshot g_gh3018_goodix_hrspo2_snapshot;

static GS32 s_rawData[GH3018_GOODIX_HRSPO2_RAW_FRAME_CAPACITY][6];
static ST_HB_RES s_hbResult;
static ST_HRV_RES s_hrvResult;
static uint8_t s_greenRawFallbackActive;
static uint8_t s_greenRawFallbackNoDataStreak;
static int32_t s_ppgLastAcceptedRaw;
static uint8_t s_ppgHasLastAcceptedRaw;
static uint8_t s_ppgRawJumpRejectStreak;
static uint32_t s_ppgLastRawTickMs;
static uint32_t s_ppgLastCandidateTickMs;
static uint32_t s_ppgLastIntervalTickMs;
static uint32_t s_ppgLastCandidatePeakCount;
static uint32_t s_ppgLastIntervalAcceptedCount;
static uint32_t s_ppgWearStateChangeTick;
static uint32_t s_ppgWearHighStartTick;
static uint32_t s_ppgWearLowStartTick;
static uint8_t s_ppgHasLastRawTick;
static uint8_t s_ppgHasLastCandidateTick;
static uint8_t s_ppgHasLastIntervalTick;
static uint8_t s_ppgWearHighActive;
static uint8_t s_ppgWearLowActive;
static uint8_t s_ppgIntervalExpiryActive;
static int32_t s_ppgWearDc;
static int32_t s_ppgWearEmptyDc;
static uint8_t s_ppgWearHasDc;
static uint8_t s_ppgWearHasEmptyDc;
static uint32_t s_spo2SimRng;
static uint32_t s_spo2SimNextSwitchTick;
static uint8_t s_spo2SimSeeded;
static uint8_t s_manualWearOverride;
static uint32_t s_manualWearStartTick;

static int16_t Gh3018GoodixHrSpo2_CurrentStepToX10(uint8_t step)
{
    return (int16_t)((uint16_t)step * 4U);
}

static int16_t Gh3018GoodixHrSpo2_CurrentMaToX10(GF32 currentMa)
{
    if (currentMa >= 0.0f) {
        return (int16_t)((currentMa * 10.0f) + 0.5f);
    }
    return (int16_t)((currentMa * 10.0f) - 0.5f);
}

static uint32_t Gh3018GoodixHrSpo2_Abs32(int32_t value)
{
    if (value < 0) {
        return (uint32_t)(-value);
    }
    return (uint32_t)value;
}

static uint32_t Gh3018GoodixHrSpo2_MaxU32(uint32_t a, uint32_t b)
{
    return (a > b) ? a : b;
}

static void Gh3018GoodixHrSpo2_UpdateWearDc(int32_t rawPpg0)
{
    if (s_ppgWearHasDc == 0U) {
        s_ppgWearDc = rawPpg0;
        s_ppgWearHasDc = 1U;
    } else {
        s_ppgWearDc +=
            (rawPpg0 - s_ppgWearDc) /
            GH3018_GOODIX_HRSPO2_WEAR_DC_FILTER_DIV;
    }
}

static uint32_t Gh3018GoodixHrSpo2_CalcWearOnDcThreshold(int32_t emptyDc)
{
    return Gh3018GoodixHrSpo2_MaxU32(
        GH3018_GOODIX_HRSPO2_WEAR_ON_DC_THRESHOLD_MIN,
        Gh3018GoodixHrSpo2_Abs32(emptyDc) /
            GH3018_GOODIX_HRSPO2_WEAR_ON_DC_THRESHOLD_DIV);
}

static uint32_t Gh3018GoodixHrSpo2_CalcWearOffDcThreshold(int32_t emptyDc)
{
    return Gh3018GoodixHrSpo2_MaxU32(
        GH3018_GOODIX_HRSPO2_WEAR_OFF_DC_THRESHOLD_MIN,
        Gh3018GoodixHrSpo2_Abs32(emptyDc) /
            GH3018_GOODIX_HRSPO2_WEAR_OFF_DC_THRESHOLD_DIV);
}

static uint32_t Gh3018GoodixHrSpo2_Spo2SimNextRandom(void)
{
    if (s_spo2SimSeeded == 0U) {
        s_spo2SimRng =
            HAL_GetTick() ^
            (uint32_t)g_gh3018_goodix_hrspo2_snapshot.rawPpg0 ^
            (g_gh3018_goodix_hrspo2_snapshot.sessionId << 16) ^
            0xA5A55A5AU;
        if (s_spo2SimRng == 0U) {
            s_spo2SimRng = 0x13579BDFU;
        }
        s_spo2SimSeeded = 1U;
    }

    s_spo2SimRng ^= s_spo2SimRng << 13;
    s_spo2SimRng ^= s_spo2SimRng >> 17;
    s_spo2SimRng ^= s_spo2SimRng << 5;
    if (s_spo2SimRng == 0U) {
        s_spo2SimRng = 0x2468ACE1U;
    }
    return s_spo2SimRng;
}

static uint32_t Gh3018GoodixHrSpo2_CalcRawJumpThreshold(int32_t raw)
{
    uint32_t dynamicThreshold =
        Gh3018GoodixHrSpo2_Abs32(raw) /
        GH3018_GOODIX_HRSPO2_PPG_RAW_JUMP_DIVISOR;

    if (dynamicThreshold < GH3018_GOODIX_HRSPO2_PPG_RAW_JUMP_MIN_THRESHOLD) {
        return GH3018_GOODIX_HRSPO2_PPG_RAW_JUMP_MIN_THRESHOLD;
    }
    return dynamicThreshold;
}

static uint32_t Gh3018GoodixHrSpo2_AgeMs(
    uint8_t hasTick,
    uint32_t tick,
    uint32_t now)
{
    if (hasTick == 0U) {
        return GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    }
    return now - tick;
}

static void Gh3018GoodixHrSpo2_SetPpgWearState(
    uint8_t wearing,
    uint32_t now)
{
    if (g_gh3018_goodix_hrspo2_snapshot.ppgWearingState == wearing) {
        return;
    }

    g_gh3018_goodix_hrspo2_snapshot.ppgWearingState = wearing;
    g_gh3018_goodix_hrspo2_snapshot.wearingState = wearing;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearStableMs = 0U;
    s_ppgWearStateChangeTick = now;
    s_ppgWearHighActive = 0U;
    s_ppgWearLowActive = 0U;
}

static void Gh3018GoodixHrSpo2_ApplyManualWearOverride(uint32_t now)
{
    if (s_manualWearOverride == 0U) {
        return;
    }

    g_gh3018_goodix_hrspo2_snapshot.ppgWearingState = 1U;
    g_gh3018_goodix_hrspo2_snapshot.wearingState = 1U;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearScore = 100U;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearReason =
        GH3018_GOODIX_HRSPO2_WEAR_REASON_MANUAL;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearStableMs =
        now - s_manualWearStartTick;
    s_ppgWearHighActive = 0U;
    s_ppgWearLowActive = 0U;
}

static void Gh3018GoodixHrSpo2_UpdatePpgWear(
    uint32_t now,
    const Gh3018GreenPpgBpmSnapshot *ppg)
{
    uint32_t rawAge;
    uint32_t candidateAge;
    uint32_t intervalAge;
    uint32_t peakAge;
    uint32_t darkDelta = 0U;
    uint32_t wearOnThreshold =
        GH3018_GOODIX_HRSPO2_WEAR_ON_DC_THRESHOLD_MIN;
    uint32_t wearOffThreshold =
        GH3018_GOODIX_HRSPO2_WEAR_OFF_DC_THRESHOLD_MIN;
    uint32_t score = 0U;
    uint8_t reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_NONE;
    uint8_t rawTimedOut;
    uint8_t rawRecent;
    uint8_t sampleReady;
    uint8_t motionFrozen;
    uint8_t rawJumping;
    uint8_t hasBaseline;
    uint8_t wearCandidate;
    uint8_t unwearCandidate;

    if (s_ppgWearStateChangeTick == 0U) {
        s_ppgWearStateChangeTick = now;
    }

    if (ppg != NULL) {
        if (ppg->candidatePeakCount > s_ppgLastCandidatePeakCount) {
            s_ppgLastCandidateTickMs = now;
            s_ppgHasLastCandidateTick = 1U;
        }
        s_ppgLastCandidatePeakCount = ppg->candidatePeakCount;
        if (ppg->intervalAcceptedCount > s_ppgLastIntervalAcceptedCount) {
            s_ppgLastIntervalTickMs = now;
            s_ppgHasLastIntervalTick = 1U;
            s_ppgIntervalExpiryActive = 0U;
        }
        s_ppgLastIntervalAcceptedCount = ppg->intervalAcceptedCount;
    }

    rawAge = Gh3018GoodixHrSpo2_AgeMs(
        s_ppgHasLastRawTick,
        s_ppgLastRawTickMs,
        now);
    candidateAge = Gh3018GoodixHrSpo2_AgeMs(
        s_ppgHasLastCandidateTick,
        s_ppgLastCandidateTickMs,
        now);
    intervalAge = Gh3018GoodixHrSpo2_AgeMs(
        s_ppgHasLastIntervalTick,
        s_ppgLastIntervalTickMs,
        now);
    if ((intervalAge != GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID) &&
        (intervalAge > GH3018_GOODIX_HRSPO2_BPM_INTERVAL_CLEAR_MS)) {
        if (s_ppgIntervalExpiryActive == 0U) {
            Gh3018GreenPpgBpm_ClearIntervalHistory();
            g_gh3018_goodix_hrspo2_snapshot.ppgIntervalExpiredCount++;
            s_ppgIntervalExpiryActive = 1U;
        }
        s_ppgHasLastIntervalTick = 0U;
        intervalAge = GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    }
    peakAge = intervalAge;
    motionFrozen =
        ((ppg != NULL) && (ppg->motionFreeze != 0U)) ? 1U : 0U;
    rawJumping = (s_ppgRawJumpRejectStreak != 0U) ? 1U : 0U;
    rawTimedOut =
        ((rawAge == GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID) ||
         (rawAge > GH3018_GOODIX_HRSPO2_WEAR_RAW_TIMEOUT_MS)) ? 1U : 0U;
    rawRecent =
        ((rawTimedOut == 0U) &&
         (rawAge <= GH3018_GOODIX_HRSPO2_WEAR_RAW_RECENT_MS)) ? 1U : 0U;
    sampleReady =
        (g_gh3018_goodix_hrspo2_snapshot.ppgRawAcceptedCount >=
         GH3018_GOODIX_HRSPO2_WEAR_MIN_RAW_ACCEPTED) ? 1U : 0U;

    if ((g_gh3018_goodix_hrspo2_snapshot.ppgWearingState == 0U) &&
        (rawRecent != 0U) &&
        (s_ppgWearHasDc != 0U) &&
        (motionFrozen == 0U) &&
        (rawJumping == 0U) &&
        (s_ppgWearHasEmptyDc == 0U)) {
        s_ppgWearEmptyDc = s_ppgWearDc;
        s_ppgWearHasEmptyDc = 1U;
    }

    hasBaseline =
        ((s_ppgWearHasDc != 0U) &&
         (s_ppgWearHasEmptyDc != 0U)) ? 1U : 0U;
    if (hasBaseline != 0U) {
        int32_t delta = s_ppgWearDc - s_ppgWearEmptyDc;
        if (delta < 0) {
            darkDelta = Gh3018GoodixHrSpo2_Abs32(delta);
        }
        wearOnThreshold =
            Gh3018GoodixHrSpo2_CalcWearOnDcThreshold(s_ppgWearEmptyDc);
        wearOffThreshold =
            Gh3018GoodixHrSpo2_CalcWearOffDcThreshold(s_ppgWearEmptyDc);
        g_gh3018_goodix_hrspo2_snapshot.ppgWearDcDelta = delta;

        if ((g_gh3018_goodix_hrspo2_snapshot.ppgWearingState == 0U) &&
            (s_ppgWearHighActive == 0U) &&
            (rawRecent != 0U) &&
            (motionFrozen == 0U) &&
            (rawJumping == 0U) &&
            (darkDelta <= wearOffThreshold)) {
            s_ppgWearEmptyDc +=
                (s_ppgWearDc - s_ppgWearEmptyDc) /
                GH3018_GOODIX_HRSPO2_WEAR_EMPTY_DC_FILTER_DIV;

            delta = s_ppgWearDc - s_ppgWearEmptyDc;
            darkDelta = (delta < 0) ?
                Gh3018GoodixHrSpo2_Abs32(delta) :
                0U;
            wearOnThreshold =
                Gh3018GoodixHrSpo2_CalcWearOnDcThreshold(
                    s_ppgWearEmptyDc);
            wearOffThreshold =
                Gh3018GoodixHrSpo2_CalcWearOffDcThreshold(
                    s_ppgWearEmptyDc);
            g_gh3018_goodix_hrspo2_snapshot.ppgWearDcDelta = delta;
        }
    } else {
        g_gh3018_goodix_hrspo2_snapshot.ppgWearDcDelta = 0;
    }
    g_gh3018_goodix_hrspo2_snapshot.ppgWearEmptyDc =
        (s_ppgWearHasEmptyDc != 0U) ? s_ppgWearEmptyDc : 0;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearDcThreshold =
        wearOnThreshold;

    wearCandidate =
        ((rawRecent != 0U) &&
         (sampleReady != 0U) &&
         (hasBaseline != 0U) &&
         (darkDelta >= wearOnThreshold) &&
         (motionFrozen == 0U) &&
         (rawJumping == 0U)) ? 1U : 0U;

    if (rawTimedOut != 0U) {
        score = 0U;
        reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_RAW_TIMEOUT;
    } else if (motionFrozen != 0U) {
        score = 0U;
        reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_MOTION_FREEZE;
    } else if (rawJumping != 0U) {
        score = 0U;
        reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_LOW_SCORE;
    } else {
        if (rawRecent != 0U) {
            score += 20U;
            reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_RAW_RECENT;
        }
        if (sampleReady != 0U) {
            score += 20U;
        }
        if ((hasBaseline != 0U) && (darkDelta > wearOffThreshold)) {
            score += 20U;
            reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_DC_DELTA;
        }
        if ((hasBaseline != 0U) && (darkDelta >= wearOnThreshold)) {
            score += 40U;
            reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_DC_DELTA;
        }
        if (wearCandidate != 0U) {
            if (score < 80U) {
                score = 80U;
            }
            reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_DC_DELTA;
        }
        if (score == 0U) {
            reason = GH3018_GOODIX_HRSPO2_WEAR_REASON_LOW_SCORE;
        }
    }

    if (score > 100U) {
        score = 100U;
    }
    g_gh3018_goodix_hrspo2_snapshot.ppgWearScore = (uint8_t)score;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearReason = reason;
    g_gh3018_goodix_hrspo2_snapshot.ppgRawAgeMs = rawAge;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakAgeMs = peakAge;
    g_gh3018_goodix_hrspo2_snapshot.ppgCandidateAgeMs = candidateAge;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalAgeMs = intervalAge;

    if ((rawTimedOut != 0U) &&
        (g_gh3018_goodix_hrspo2_snapshot.ppgWearingState != 0U)) {
        unwearCandidate = 1U;
    } else if (g_gh3018_goodix_hrspo2_snapshot.ppgWearingState == 0U) {
        unwearCandidate = 0U;
        if ((wearCandidate != 0U) &&
            (score >= GH3018_GOODIX_HRSPO2_WEAR_ON_SCORE)) {
            if (s_ppgWearHighActive == 0U) {
                s_ppgWearHighStartTick = now;
                s_ppgWearHighActive = 1U;
            } else if ((now - s_ppgWearHighStartTick) >=
                       GH3018_GOODIX_HRSPO2_WEAR_ON_DEBOUNCE_MS) {
                Gh3018GoodixHrSpo2_SetPpgWearState(1U, now);
            }
        } else {
            s_ppgWearHighActive = 0U;
        }
    } else {
        unwearCandidate =
            ((rawRecent == 0U) ||
             (motionFrozen != 0U) ||
             (rawJumping != 0U) ||
             (hasBaseline == 0U) ||
             ((hasBaseline != 0U) &&
              (darkDelta <= wearOffThreshold))) ? 1U : 0U;
    }

    if (g_gh3018_goodix_hrspo2_snapshot.ppgWearingState != 0U) {
        if ((unwearCandidate != 0U) ||
            (score <= GH3018_GOODIX_HRSPO2_WEAR_OFF_SCORE)) {
            if (s_ppgWearLowActive == 0U) {
                s_ppgWearLowStartTick = now;
                s_ppgWearLowActive = 1U;
            } else if ((now - s_ppgWearLowStartTick) >=
                       GH3018_GOODIX_HRSPO2_WEAR_OFF_DEBOUNCE_MS) {
                Gh3018GoodixHrSpo2_SetPpgWearState(0U, now);
            }
        } else {
            s_ppgWearLowActive = 0U;
        }
    } else {
        s_ppgWearLowActive = 0U;
    }

    g_gh3018_goodix_hrspo2_snapshot.wearingState =
        g_gh3018_goodix_hrspo2_snapshot.ppgWearingState;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearStableMs =
        now - s_ppgWearStateChangeTick;
    Gh3018GoodixHrSpo2_ApplyManualWearOverride(now);

    if (rawTimedOut != 0U) {
        g_gh3018_goodix_hrspo2_snapshot.ppgHeartRate = 0U;
        g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateValid = 0U;
        g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateConfidence = 0U;
        g_gh3018_goodix_hrspo2_snapshot.heartRate = 0U;
        g_gh3018_goodix_hrspo2_snapshot.heartRateValid = 0U;
        g_gh3018_goodix_hrspo2_snapshot.heartRateConfidence = 0U;
        g_gh3018_goodix_hrspo2_snapshot.ppgBpmGateReason =
            GH3018_GOODIX_HRSPO2_BPM_GATE_RAW_TIMEOUT;
    }
}

static void Gh3018GoodixHrSpo2_ClearPpgBpm(
    Gh3018GoodixHrSpo2BpmGateReason reason)
{
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRate = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateConfidence = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateValid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.heartRate = 0U;
    g_gh3018_goodix_hrspo2_snapshot.heartRateConfidence = 0U;
    g_gh3018_goodix_hrspo2_snapshot.heartRateValid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgBpmGateReason = (uint8_t)reason;
}

static uint8_t Gh3018GoodixHrSpo2_TickReached(
    uint32_t now,
    uint32_t target)
{
    return (((int32_t)(now - target)) >= 0) ? 1U : 0U;
}

static void Gh3018GoodixHrSpo2_ClearSpo2Sim(void)
{
    g_gh3018_goodix_hrspo2_snapshot.spo2 = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2Confidence = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2Valid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2Simulated = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2RValue = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2ValidLevel = 0;
    g_gh3018_goodix_hrspo2_snapshot.spo2InvalidFlag = 1;
    g_gh3018_goodix_hrspo2_snapshot.spo2SimNextSwitchMs = 0U;
    s_spo2SimNextSwitchTick = 0U;
}

static void Gh3018GoodixHrSpo2_UpdateSpo2Sim(uint32_t now)
{
    uint32_t randomValue;

    if (g_gh3018_goodix_hrspo2_snapshot.ppgWearingState == 0U) {
        Gh3018GoodixHrSpo2_ClearSpo2Sim();
        return;
    }

    if ((g_gh3018_goodix_hrspo2_snapshot.spo2Valid == 0U) ||
        (s_spo2SimNextSwitchTick == 0U) ||
        (Gh3018GoodixHrSpo2_TickReached(
             now,
             s_spo2SimNextSwitchTick) != 0U)) {
        randomValue = Gh3018GoodixHrSpo2_Spo2SimNextRandom();
        g_gh3018_goodix_hrspo2_snapshot.spo2 =
            (uint8_t)(98U + (randomValue & 1U));
        g_gh3018_goodix_hrspo2_snapshot.spo2Confidence = 100U;
        g_gh3018_goodix_hrspo2_snapshot.spo2Valid = 1U;
        g_gh3018_goodix_hrspo2_snapshot.spo2Simulated = 1U;
        g_gh3018_goodix_hrspo2_snapshot.spo2InvalidFlag = 0;
        g_gh3018_goodix_hrspo2_snapshot.spo2ValidLevel = 100;
        g_gh3018_goodix_hrspo2_snapshot.spo2RValue = 0U;
        g_gh3018_goodix_hrspo2_snapshot.spo2SimSwitchCount++;
        s_spo2SimNextSwitchTick =
            now + GH3018_GOODIX_HRSPO2_SPO2_SIM_SWITCH_MS;
    }

    g_gh3018_goodix_hrspo2_snapshot.spo2SimNextSwitchMs =
        (s_spo2SimNextSwitchTick != 0U) ?
            (s_spo2SimNextSwitchTick - now) :
            0U;
}

static void Gh3018GoodixHrSpo2_ApplyBpmGate(
    const Gh3018GreenPpgBpmSnapshot *ppg)
{
    uint8_t bpm;

    if (ppg == NULL) {
        Gh3018GoodixHrSpo2_ClearPpgBpm(
            GH3018_GOODIX_HRSPO2_BPM_GATE_PPG_INVALID);
        return;
    }
    bpm = ppg->bpm;
    if ((bpm < GH3018_GOODIX_HRSPO2_BPM_MIN) ||
        (bpm > GH3018_GOODIX_HRSPO2_BPM_MAX)) {
        Gh3018GoodixHrSpo2_ClearPpgBpm(
            GH3018_GOODIX_HRSPO2_BPM_GATE_PPG_INVALID);
        return;
    }
    if (s_manualWearOverride == 0U) {
        Gh3018GoodixHrSpo2_ClearPpgBpm(
            GH3018_GOODIX_HRSPO2_BPM_GATE_WEAR_OFF);
        return;
    }

    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRate = bpm;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateConfidence =
        ppg->confidence;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateValid = 1U;
    g_gh3018_goodix_hrspo2_snapshot.heartRate = bpm;
    g_gh3018_goodix_hrspo2_snapshot.heartRateConfidence =
        ppg->confidence;
    g_gh3018_goodix_hrspo2_snapshot.heartRateValid = 1U;
    g_gh3018_goodix_hrspo2_snapshot.ppgBpmGateReason =
        GH3018_GOODIX_HRSPO2_BPM_GATE_NONE;
}

static uint8_t Gh3018GoodixHrSpo2_FilterPpgRaw(
    int32_t rawPpg0,
    uint32_t now)
{
    uint32_t threshold;
    uint32_t delta;

    if (rawPpg0 <= 0) {
        g_gh3018_goodix_hrspo2_snapshot.ppgRawRejectedZeroCount++;
        s_ppgRawJumpRejectStreak = 0U;
        return 0U;
    }

    if (s_ppgHasLastAcceptedRaw == 0U) {
        s_ppgLastAcceptedRaw = rawPpg0;
        s_ppgHasLastAcceptedRaw = 1U;
        s_ppgRawJumpRejectStreak = 0U;
        g_gh3018_goodix_hrspo2_snapshot.ppgLastAcceptedRaw = rawPpg0;
        g_gh3018_goodix_hrspo2_snapshot.ppgRawJumpThreshold =
            Gh3018GoodixHrSpo2_CalcRawJumpThreshold(rawPpg0);
        g_gh3018_goodix_hrspo2_snapshot.ppgRawAcceptedCount++;
        s_ppgLastRawTickMs = now;
        s_ppgHasLastRawTick = 1U;
        Gh3018GoodixHrSpo2_UpdateWearDc(rawPpg0);
        return 1U;
    }

    threshold = Gh3018GoodixHrSpo2_CalcRawJumpThreshold(s_ppgLastAcceptedRaw);
    delta = Gh3018GoodixHrSpo2_Abs32(rawPpg0 - s_ppgLastAcceptedRaw);
    g_gh3018_goodix_hrspo2_snapshot.ppgRawJumpThreshold = threshold;
    if (delta > threshold) {
        g_gh3018_goodix_hrspo2_snapshot.ppgRawRejectedJumpCount++;
        s_ppgRawJumpRejectStreak++;
        if (s_ppgRawJumpRejectStreak <
            GH3018_GOODIX_HRSPO2_PPG_RAW_RESYNC_REJECTS) {
            return 0U;
        }

        Gh3018GreenPpgBpm_Reset();
        g_gh3018_goodix_hrspo2_snapshot.ppgRawAcceptedCount = 0U;
        g_gh3018_goodix_hrspo2_snapshot.ppgRawResyncCount++;
        s_ppgLastCandidatePeakCount = 0U;
        s_ppgLastIntervalAcceptedCount = 0U;
        s_ppgHasLastCandidateTick = 0U;
        s_ppgHasLastIntervalTick = 0U;
        s_ppgIntervalExpiryActive = 0U;
        s_ppgRawJumpRejectStreak = 0U;
    } else {
        s_ppgRawJumpRejectStreak = 0U;
    }

    s_ppgLastAcceptedRaw = rawPpg0;
    g_gh3018_goodix_hrspo2_snapshot.ppgLastAcceptedRaw = rawPpg0;
    g_gh3018_goodix_hrspo2_snapshot.ppgRawJumpThreshold =
        Gh3018GoodixHrSpo2_CalcRawJumpThreshold(rawPpg0);
    g_gh3018_goodix_hrspo2_snapshot.ppgRawAcceptedCount++;
    s_ppgLastRawTickMs = now;
    s_ppgHasLastRawTick = 1U;
    Gh3018GoodixHrSpo2_UpdateWearDc(rawPpg0);
    return 1U;
}

static void Gh3018GoodixHrSpo2_UpdatePins(void)
{
    g_gh3018_goodix_hrspo2_snapshot.rstnLevel =
        (uint8_t)HAL_GPIO_ReadPin(GH3018_RSTN_GPIO_Port, GH3018_RSTN_Pin);
    g_gh3018_goodix_hrspo2_snapshot.hbdOnLevel =
        (uint8_t)HAL_GPIO_ReadPin(GH3018_HBD_ON_GPIO_Port, GH3018_HBD_ON_Pin);
    g_gh3018_goodix_hrspo2_snapshot.intLevel =
        (uint8_t)HAL_GPIO_ReadPin(GH3018_INT_GPIO_Port, GH3018_INT_Pin);
}

static void Gh3018GoodixHrSpo2_CopyCommSnapshot(const Gh3018CommSnapshot *comm)
{
    if (comm == NULL) {
        return;
    }

    g_gh3018_goodix_hrspo2_snapshot.commStatus = comm->status;
    g_gh3018_goodix_hrspo2_snapshot.lastSoftI2cStatus =
        comm->lastSoftI2cStatus;
    g_gh3018_goodix_hrspo2_snapshot.i2cWriteCount = comm->i2cWriteCount;
    g_gh3018_goodix_hrspo2_snapshot.i2cReadCount = comm->i2cReadCount;
    g_gh3018_goodix_hrspo2_snapshot.busRecoveryCount =
        comm->busRecoveryCount;
    g_gh3018_goodix_hrspo2_snapshot.addressNackCount =
        comm->addressNackCount;
    g_gh3018_goodix_hrspo2_snapshot.dataNackCount = comm->dataNackCount;
    g_gh3018_goodix_hrspo2_snapshot.sclTimeoutCount =
        comm->sclTimeoutCount;
}

static void Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot(void)
{
    Gh3018GoodixHrSpo2_CopyCommSnapshot(Gh3018Comm_GetSnapshot());
    Gh3018GoodixHrSpo2_UpdatePins();
}

static void Gh3018GoodixHrSpo2_ForceWearForGreenValidation(void)
{
    GF32 wearDirection[3] = {0.0f, 0.0f, 1.0f};

    HBD_EnableWearing(wearDirection);
}

static void Gh3018GoodixHrSpo2_ResetSessionData(void)
{
    memset(s_rawData, 0, sizeof(s_rawData));
    memset(&s_hbResult, 0, sizeof(s_hbResult));
    memset(&s_hrvResult, 0, sizeof(s_hrvResult));
    Gh3018GreenPpgBpm_Reset();
    s_greenRawFallbackActive = 0U;
    s_greenRawFallbackNoDataStreak = 0U;
    s_ppgLastAcceptedRaw = 0;
    s_ppgHasLastAcceptedRaw = 0U;
    s_ppgRawJumpRejectStreak = 0U;
    s_ppgLastRawTickMs = 0U;
    s_ppgLastCandidateTickMs = 0U;
    s_ppgLastIntervalTickMs = 0U;
    s_ppgLastCandidatePeakCount = 0U;
    s_ppgLastIntervalAcceptedCount = 0U;
    s_ppgWearStateChangeTick = HAL_GetTick();
    s_ppgWearHighStartTick = 0U;
    s_ppgWearLowStartTick = 0U;
    s_ppgHasLastRawTick = 0U;
    s_ppgHasLastCandidateTick = 0U;
    s_ppgHasLastIntervalTick = 0U;
    s_ppgWearHighActive = 0U;
    s_ppgWearLowActive = 0U;
    s_ppgIntervalExpiryActive = 0U;
    s_ppgWearDc = 0;
    s_ppgWearEmptyDc = 0;
    s_ppgWearHasDc = 0U;
    s_ppgWearHasEmptyDc = 0U;
    s_spo2SimRng = 0U;
    s_spo2SimNextSwitchTick = 0U;
    s_spo2SimSeeded = 0U;
    s_manualWearOverride = 0U;
    s_manualWearStartTick = 0U;

    g_gh3018_goodix_hrspo2_snapshot.hbdCalcRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_goodix_hrspo2_snapshot.intStatus = INT_STATUS_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.heartRate = 0U;
    g_gh3018_goodix_hrspo2_snapshot.heartRateConfidence = 0U;
    g_gh3018_goodix_hrspo2_snapshot.heartRateValid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRate = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateConfidence = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateValid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgSignalQuality = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBpm = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrScore = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrValid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBestScore = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrSecondScore = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrRejectReason = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgMotionFreeze = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgDisplayedBpm = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgPendingBpm = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrLag = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBestLag = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrSecondLag = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgDcDrift1s = 0;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearingState = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearScore = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearReason =
        GH3018_GOODIX_HRSPO2_WEAR_REASON_NONE;
    g_gh3018_goodix_hrspo2_snapshot.ppgSampleCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAcRange = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgLastIntervalMs = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgCandidatePeakCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalAcceptedCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalTooShortCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalTooLongCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalRejectedOutlierCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgLastRejectedIntervalMs = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakThreshold = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgRawAcceptedCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgRawRejectedZeroCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgRawRejectedJumpCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgRawResyncCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgLastAcceptedRaw = 0;
    g_gh3018_goodix_hrspo2_snapshot.ppgRawJumpThreshold = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearEmptyDc = 0;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearDcDelta = 0;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearDcThreshold =
        GH3018_GOODIX_HRSPO2_WEAR_ON_DC_THRESHOLD_MIN;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearStableMs = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgRawAgeMs =
        GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakAgeMs =
        GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.ppgCandidateAgeMs =
        GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalAgeMs =
        GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalExpiredCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgPendingPeakExpiredCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgBpmGateReason =
        GH3018_GOODIX_HRSPO2_BPM_GATE_PPG_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.goodixHeartRate = 0U;
    g_gh3018_goodix_hrspo2_snapshot.goodixHeartRateConfidence = 0U;
    g_gh3018_goodix_hrspo2_snapshot.goodixHeartRateValid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.goodixWearingState = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2 = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2Confidence = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2Valid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2Simulated = 0U;
    g_gh3018_goodix_hrspo2_snapshot.wearingState = 0U;
    g_gh3018_goodix_hrspo2_snapshot.hrvCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.hrvConfidence = 0U;
    g_gh3018_goodix_hrspo2_snapshot.spo2RValue = 0U;
    memset(g_gh3018_goodix_hrspo2_snapshot.hrvRR,
           0,
           sizeof(g_gh3018_goodix_hrspo2_snapshot.hrvRR));
    g_gh3018_goodix_hrspo2_snapshot.spo2ValidLevel = 0;
    g_gh3018_goodix_hrspo2_snapshot.spo2InvalidFlag = 1;
    g_gh3018_goodix_hrspo2_snapshot.rawDataLen = 0U;
    g_gh3018_goodix_hrspo2_snapshot.rawFifoCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.rawPpg0 = 0;
    g_gh3018_goodix_hrspo2_snapshot.rawPpg1 = 0;
    g_gh3018_goodix_hrspo2_snapshot.rawMaxPpg0 = 0U;
    g_gh3018_goodix_hrspo2_snapshot.rawMaxPpg1 = 0U;
    g_gh3018_goodix_hrspo2_snapshot.rawProbeCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.rawNonzeroCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.rawChangeCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.rawEmptyCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.rawBufferFullCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.sessionPollCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.sessionCalcCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.sessionResultRefreshCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.sessionNoDataCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.lastResultTick = 0U;
}

static void Gh3018GoodixHrSpo2_CountInterrupt(uint8_t intStatus)
{
    switch (intStatus) {
    case INT_STATUS_CHIP_RESET:
        g_gh3018_goodix_hrspo2_snapshot.intChipResetCount++;
        break;
    case INT_STATUS_NEW_DATA:
        g_gh3018_goodix_hrspo2_snapshot.intNewDataCount++;
        break;
    case INT_STATUS_FIFO_WATERMARK:
        g_gh3018_goodix_hrspo2_snapshot.intFifoWatermarkCount++;
        break;
    case INT_STATUS_FIFO_FULL:
        g_gh3018_goodix_hrspo2_snapshot.intFifoFullCount++;
        break;
    case INT_STATUS_WEAR_DETECTED:
        g_gh3018_goodix_hrspo2_snapshot.intWearCount++;
        break;
    case INT_STATUS_UNWEAR_DETECTED:
        g_gh3018_goodix_hrspo2_snapshot.intUnwearCount++;
        break;
    case INT_STATUS_INVALID:
    default:
        g_gh3018_goodix_hrspo2_snapshot.intInvalidCount++;
        break;
    }
}

static uint8_t Gh3018GoodixHrSpo2_IntHasData(uint8_t intStatus)
{
    return (intStatus == INT_STATUS_NEW_DATA) ||
           (intStatus == INT_STATUS_FIFO_WATERMARK) ||
           (intStatus == INT_STATUS_FIFO_FULL) ||
           (intStatus == INT_STATUS_WEAR_DETECTED) ||
           (intStatus == INT_STATUS_UNWEAR_DETECTED);
}

static void Gh3018GoodixHrSpo2_UpdateRawDiagnostics(GU16 rawLen)
{
    GU16 processLen = rawLen;

    g_gh3018_goodix_hrspo2_snapshot.rawProbeCount++;
    g_gh3018_goodix_hrspo2_snapshot.rawFifoCount = HBD_GetFifoCntHasRead();
    if (rawLen == 0U) {
        g_gh3018_goodix_hrspo2_snapshot.rawEmptyCount++;
    }
    if (rawLen >= GH3018_GOODIX_HRSPO2_RAW_FRAME_CAPACITY) {
        g_gh3018_goodix_hrspo2_snapshot.rawBufferFullCount++;
    }
    if (processLen > GH3018_GOODIX_HRSPO2_RAW_FRAME_CAPACITY) {
        processLen = GH3018_GOODIX_HRSPO2_RAW_FRAME_CAPACITY;
    }

    for (GU16 i = 0U; i < processLen; ++i) {
        int32_t ppg0 = (int32_t)s_rawData[i][0];
        int32_t ppg1 = (int32_t)s_rawData[i][1];

        if ((ppg0 != g_gh3018_goodix_hrspo2_snapshot.rawPpg0) ||
            (ppg1 != g_gh3018_goodix_hrspo2_snapshot.rawPpg1)) {
            g_gh3018_goodix_hrspo2_snapshot.rawChangeCount++;
        }
        if ((ppg0 != 0) || (ppg1 != 0)) {
            g_gh3018_goodix_hrspo2_snapshot.rawNonzeroCount++;
        }
        if ((ppg0 > 0) &&
            ((uint32_t)ppg0 >
             g_gh3018_goodix_hrspo2_snapshot.rawMaxPpg0)) {
            g_gh3018_goodix_hrspo2_snapshot.rawMaxPpg0 = (uint32_t)ppg0;
        }
        if ((ppg1 > 0) &&
            ((uint32_t)ppg1 >
             g_gh3018_goodix_hrspo2_snapshot.rawMaxPpg1)) {
            g_gh3018_goodix_hrspo2_snapshot.rawMaxPpg1 = (uint32_t)ppg1;
        }
        g_gh3018_goodix_hrspo2_snapshot.rawPpg0 = ppg0;
        g_gh3018_goodix_hrspo2_snapshot.rawPpg1 = ppg1;
    }
}

static void Gh3018GoodixHrSpo2_CopyGoodixResult(uint32_t now)
{
    uint8_t hrvCount = s_hrvResult.uchRRvalueCnt;

    if (hrvCount > 4U) {
        hrvCount = 4U;
    }

    g_gh3018_goodix_hrspo2_snapshot.goodixHeartRate = s_hbResult.uchHbValue;
    g_gh3018_goodix_hrspo2_snapshot.goodixHeartRateConfidence =
        s_hbResult.uchAccuracyLevel;
    g_gh3018_goodix_hrspo2_snapshot.goodixWearingState =
        s_hbResult.uchWearingState;
    g_gh3018_goodix_hrspo2_snapshot.goodixHeartRateValid =
        ((s_hbResult.uchWearingState == 1U) &&
         (s_hbResult.uchHbValue != 0U) &&
         (s_hbResult.uchAccuracyLevel != 0U)) ? 1U : 0U;
    g_gh3018_goodix_hrspo2_snapshot.hrvCount = hrvCount;
    g_gh3018_goodix_hrspo2_snapshot.hrvConfidence =
        s_hrvResult.uchHrvConfidentLvl;

    for (uint8_t i = 0U; i < 4U; ++i) {
        g_gh3018_goodix_hrspo2_snapshot.hrvRR[i] =
            (i < hrvCount) ? s_hrvResult.usRRvalueArr[i] : 0U;
    }

    g_gh3018_goodix_hrspo2_snapshot.lastResultTick = now;
}

static void Gh3018GoodixHrSpo2_CopyPpgResult(uint32_t now)
{
    const Gh3018GreenPpgBpmSnapshot *ppg =
        Gh3018GreenPpgBpm_GetSnapshot();

    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRate = ppg->bpm;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateConfidence =
        ppg->confidence;
    g_gh3018_goodix_hrspo2_snapshot.ppgHeartRateValid = ppg->valid;
    g_gh3018_goodix_hrspo2_snapshot.ppgSignalQuality = ppg->quality;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBpm = ppg->autoCorrBpm;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrScore =
        ppg->autoCorrScore;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrValid =
        ppg->autoCorrValid;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBestScore =
        ppg->autoCorrBestScore;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrSecondScore =
        ppg->autoCorrSecondScore;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrRejectReason =
        ppg->autoCorrRejectReason;
    g_gh3018_goodix_hrspo2_snapshot.ppgMotionFreeze =
        ppg->motionFreeze;
    g_gh3018_goodix_hrspo2_snapshot.ppgDisplayedBpm =
        ppg->displayedBpm;
    g_gh3018_goodix_hrspo2_snapshot.ppgPendingBpm =
        ppg->pendingBpm;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrLag =
        ppg->autoCorrLag;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBestLag =
        ppg->autoCorrBestLag;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrSecondLag =
        ppg->autoCorrSecondLag;
    g_gh3018_goodix_hrspo2_snapshot.ppgDcDrift1s =
        ppg->dcDrift1s;
    g_gh3018_goodix_hrspo2_snapshot.ppgSampleCount = ppg->sampleCount;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakCount = ppg->peakCount;
    g_gh3018_goodix_hrspo2_snapshot.ppgAcRange = ppg->acRange;
    g_gh3018_goodix_hrspo2_snapshot.ppgLastIntervalMs =
        ppg->lastIntervalMs;
    g_gh3018_goodix_hrspo2_snapshot.ppgCandidatePeakCount =
        ppg->candidatePeakCount;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalAcceptedCount =
        ppg->intervalAcceptedCount;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalTooShortCount =
        ppg->intervalTooShortCount;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalTooLongCount =
        ppg->intervalTooLongCount;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalRejectedOutlierCount =
        ppg->intervalRejectedOutlierCount;
    g_gh3018_goodix_hrspo2_snapshot.ppgLastRejectedIntervalMs =
        ppg->lastRejectedIntervalMs;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakThreshold =
        ppg->peakThreshold;
    g_gh3018_goodix_hrspo2_snapshot.ppgPendingPeakExpiredCount =
        ppg->pendingPeakExpiredCount;

    g_gh3018_goodix_hrspo2_snapshot.heartRate = ppg->bpm;
    g_gh3018_goodix_hrspo2_snapshot.heartRateConfidence =
        ppg->confidence;
    g_gh3018_goodix_hrspo2_snapshot.heartRateValid = ppg->valid;
    Gh3018GoodixHrSpo2_UpdatePpgWear(now, ppg);
    Gh3018GoodixHrSpo2_ApplyBpmGate(ppg);
    Gh3018GoodixHrSpo2_UpdateSpo2Sim(now);
    if (g_gh3018_goodix_hrspo2_snapshot.heartRateValid != 0U) {
        g_gh3018_goodix_hrspo2_snapshot.lastResultTick = now;
    }
}

static void Gh3018GoodixHrSpo2_StartGreenRawFallback(uint32_t now)
{
    GF32 led0Current = 0.0f;
    GF32 led1Current = 0.0f;

    g_gh3018_goodix_hrspo2_snapshot.hbdStopRet = HBD_Stop();
    g_gh3018_goodix_hrspo2_snapshot.hbdHrSpo2StartRet = HBD_StartHBDOnly(
        GH3018_GOODIX_HRSPO2_GREEN_SAMPLE_RATE_HZ,
        GH3018_GOODIX_HRSPO2_GREEN_FIFO_ENABLE,
        GH3018_GOODIX_HRSPO2_GREEN_FIFO_THR);
    if (g_gh3018_goodix_hrspo2_snapshot.hbdHrSpo2StartRet != HBD_RET_OK) {
        g_gh3018_goodix_hrspo2_snapshot.status =
            GH3018_GOODIX_HRSPO2_STATUS_START_FAIL;
        g_gh3018_goodix_hrspo2_snapshot.measurementActive = 0U;
        g_gh3018_goodix_hrspo2_snapshot.lastUpdateTick = now;
        Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
        return;
    }

    s_greenRawFallbackActive = 1U;
    s_greenRawFallbackNoDataStreak = 0U;
    g_gh3018_goodix_hrspo2_snapshot.hbdSetCurrentRet = HBD_SetLedCurrent(
        GH3018_GOODIX_HRSPO2_GREEN_CURRENT_MA,
        GH3018_GOODIX_HRSPO2_RED_CURRENT_MA);
    if (g_gh3018_goodix_hrspo2_snapshot.hbdSetCurrentRet == HBD_RET_OK) {
        g_gh3018_goodix_hrspo2_snapshot.hbdGetCurrentRet =
            HBD_GetLedCurrrent(&led0Current, &led1Current);
        if (g_gh3018_goodix_hrspo2_snapshot.hbdGetCurrentRet == HBD_RET_OK) {
            g_gh3018_goodix_hrspo2_snapshot.led0CurrentX10 =
                Gh3018GoodixHrSpo2_CurrentMaToX10(led0Current);
            g_gh3018_goodix_hrspo2_snapshot.led1CurrentX10 =
                Gh3018GoodixHrSpo2_CurrentMaToX10(led1Current);
        }
    }
    g_gh3018_goodix_hrspo2_snapshot.status =
        GH3018_GOODIX_HRSPO2_STATUS_GREEN_RAW_FALLBACK;
    g_gh3018_goodix_hrspo2_snapshot.lastUpdateTick = now;
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
}

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_Init(void)
{
    HBD_INIT_CONFIG_DEFAULT_DEF(initConfig);
    const Gh3018CommSnapshot *comm;

    memset(&g_gh3018_goodix_hrspo2_snapshot,
           0,
           sizeof(g_gh3018_goodix_hrspo2_snapshot));
    memset(&s_hbResult, 0, sizeof(s_hbResult));
    memset(&s_hrvResult, 0, sizeof(s_hrvResult));
    Gh3018GreenPpgBpm_Init();
    g_gh3018_goodix_hrspo2_snapshot.status =
        GH3018_GOODIX_HRSPO2_STATUS_IDLE;
    g_gh3018_goodix_hrspo2_snapshot.hbdSimpleInitRet =
        HBD_RET_GENERIC_ERROR;
    g_gh3018_goodix_hrspo2_snapshot.hbdHrSpo2StartRet =
        HBD_RET_GENERIC_ERROR;
    g_gh3018_goodix_hrspo2_snapshot.hbdSetCurrentRet =
        HBD_RET_GENERIC_ERROR;
    g_gh3018_goodix_hrspo2_snapshot.hbdGetCurrentRet =
        HBD_RET_GENERIC_ERROR;
    g_gh3018_goodix_hrspo2_snapshot.hbdStopRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_goodix_hrspo2_snapshot.hbdCalcRet = HBD_RET_GENERIC_ERROR;
    g_gh3018_goodix_hrspo2_snapshot.intStatus = INT_STATUS_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.ledLogicMap =
        (uint8_t)HBD_LED_LOGIC_CHANNEL_MAP_PHY012;
    g_gh3018_goodix_hrspo2_snapshot.channel0CurrentStep =
        GH3018_GOODIX_HRSPO2_GREEN_CURRENT_STEP;
    g_gh3018_goodix_hrspo2_snapshot.channel1CurrentStep =
        GH3018_GOODIX_HRSPO2_RED_CURRENT_STEP;
    g_gh3018_goodix_hrspo2_snapshot.channel0CurrentX10 =
        Gh3018GoodixHrSpo2_CurrentStepToX10(
            GH3018_GOODIX_HRSPO2_GREEN_CURRENT_STEP);
    g_gh3018_goodix_hrspo2_snapshot.channel1CurrentX10 =
        Gh3018GoodixHrSpo2_CurrentStepToX10(
            GH3018_GOODIX_HRSPO2_RED_CURRENT_STEP);
    Gh3018GoodixHrSpo2_UpdatePins();

    comm = Gh3018Comm_RunSelfTest();
    Gh3018GoodixHrSpo2_CopyCommSnapshot(comm);
    Gh3018GoodixHrSpo2_UpdatePins();
    if ((comm == NULL) || (comm->status != GH3018_COMM_STATUS_CHIP_ID_OK)) {
        g_gh3018_goodix_hrspo2_snapshot.status =
            GH3018_GOODIX_HRSPO2_STATUS_COMM_FAIL;
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    initConfig.stHbInitConfig.emHbModeFifoEnable =
        HBD_FUNCTIONAL_STATE_ENABLE;
    initConfig.stHbInitConfig.emHrvModeFifoEnable =
        HBD_FUNCTIONAL_STATE_ENABLE;
    initConfig.stHbInitConfig.emBpfModeFifoEnable =
        HBD_FUNCTIONAL_STATE_DISABLE;
    initConfig.stHbInitConfig.emReserve2ModeFifoEnable =
        HBD_FUNCTIONAL_STATE_DISABLE;
    initConfig.stHbInitConfig.emReserve3ModeFifoEnable =
        HBD_FUNCTIONAL_STATE_DISABLE;
    initConfig.stHbInitConfig.emSpo2ModeFifoEnable =
        HBD_FUNCTIONAL_STATE_DISABLE;
    initConfig.stAdtInitConfig.emGINTEnable = HBD_FUNCTIONAL_STATE_ENABLE;
    initConfig.stAdtInitConfig.emLedLogicChannelMap =
        HBD_LED_LOGIC_CHANNEL_MAP_PHY012;
    initConfig.stAdtInitConfig.uchLogicChannel0Current =
        GH3018_GOODIX_HRSPO2_GREEN_CURRENT_STEP;
    initConfig.stAdtInitConfig.uchLogicChannel1Current =
        GH3018_GOODIX_HRSPO2_RED_CURRENT_STEP;
    g_gh3018_goodix_hrspo2_snapshot.hbdSimpleInitRet =
        HBD_SimpleInit(&initConfig);
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    if (g_gh3018_goodix_hrspo2_snapshot.hbdSimpleInitRet != HBD_RET_OK) {
        g_gh3018_goodix_hrspo2_snapshot.status =
            GH3018_GOODIX_HRSPO2_STATUS_SIMPLE_INIT_FAIL;
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    g_gh3018_goodix_hrspo2_snapshot.status =
        GH3018_GOODIX_HRSPO2_STATUS_READY;
    g_gh3018_goodix_hrspo2_snapshot.lastUpdateTick = HAL_GetTick();
    return &g_gh3018_goodix_hrspo2_snapshot;
}

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_Start(void)
{
    uint32_t now = HAL_GetTick();
    GF32 led0Current = 0.0f;
    GF32 led1Current = 0.0f;

    if (g_gh3018_goodix_hrspo2_snapshot.measurementActive != 0U) {
        Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
        return &g_gh3018_goodix_hrspo2_snapshot;
    }
    if ((g_gh3018_goodix_hrspo2_snapshot.commStatus !=
         GH3018_COMM_STATUS_CHIP_ID_OK) ||
        (g_gh3018_goodix_hrspo2_snapshot.hbdSimpleInitRet != HBD_RET_OK)) {
        Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    Gh3018GoodixHrSpo2_ResetSessionData();
    g_gh3018_goodix_hrspo2_snapshot.startCount++;
    g_gh3018_goodix_hrspo2_snapshot.hbdSetCurrentRet = HBD_SetLedCurrent(
        GH3018_GOODIX_HRSPO2_GREEN_CURRENT_MA,
        GH3018_GOODIX_HRSPO2_RED_CURRENT_MA);
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    if (g_gh3018_goodix_hrspo2_snapshot.hbdSetCurrentRet == HBD_RET_OK) {
        g_gh3018_goodix_hrspo2_snapshot.hbdGetCurrentRet =
            HBD_GetLedCurrrent(&led0Current, &led1Current);
        if (g_gh3018_goodix_hrspo2_snapshot.hbdGetCurrentRet == HBD_RET_OK) {
            g_gh3018_goodix_hrspo2_snapshot.led0CurrentX10 =
                Gh3018GoodixHrSpo2_CurrentMaToX10(led0Current);
            g_gh3018_goodix_hrspo2_snapshot.led1CurrentX10 =
                Gh3018GoodixHrSpo2_CurrentMaToX10(led1Current);
        }
    }

    g_gh3018_goodix_hrspo2_snapshot.hbdHrSpo2StartRet =
        HBD_HbWithHrvDetectStart();
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    if (g_gh3018_goodix_hrspo2_snapshot.hbdHrSpo2StartRet != HBD_RET_OK) {
        g_gh3018_goodix_hrspo2_snapshot.status =
            GH3018_GOODIX_HRSPO2_STATUS_START_FAIL;
        g_gh3018_goodix_hrspo2_snapshot.lastUpdateTick = now;
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    Gh3018GoodixHrSpo2_ForceWearForGreenValidation();

    g_gh3018_goodix_hrspo2_snapshot.sessionId++;
    g_gh3018_goodix_hrspo2_snapshot.measurementActive = 1U;
    g_gh3018_goodix_hrspo2_snapshot.status =
        GH3018_GOODIX_HRSPO2_STATUS_WAITING_DATA;
    g_gh3018_goodix_hrspo2_snapshot.lastUpdateTick = now;
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    return &g_gh3018_goodix_hrspo2_snapshot;
}

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_Stop(void)
{
    uint32_t now = HAL_GetTick();

    if (g_gh3018_goodix_hrspo2_snapshot.measurementActive == 0U) {
        Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    g_gh3018_goodix_hrspo2_snapshot.stopCount++;
    g_gh3018_goodix_hrspo2_snapshot.hbdStopRet = HBD_Stop();
    g_gh3018_goodix_hrspo2_snapshot.measurementActive = 0U;
    g_gh3018_goodix_hrspo2_snapshot.lastUpdateTick = now;
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    if (g_gh3018_goodix_hrspo2_snapshot.hbdStopRet == HBD_RET_OK) {
        g_gh3018_goodix_hrspo2_snapshot.status =
            GH3018_GOODIX_HRSPO2_STATUS_STOPPED;
    } else {
        g_gh3018_goodix_hrspo2_snapshot.status =
            GH3018_GOODIX_HRSPO2_STATUS_STOP_FAIL;
    }
    return &g_gh3018_goodix_hrspo2_snapshot;
}

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_Poll(void)
{
    uint32_t now = HAL_GetTick();
    GU32 rawPpg0 = 0U;
    GU32 rawPpg1 = 0U;
    GU8 rawRet = HBD_RET_OK;

    if (g_gh3018_goodix_hrspo2_snapshot.measurementActive == 0U) {
        Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    g_gh3018_goodix_hrspo2_snapshot.pollCount++;
    g_gh3018_goodix_hrspo2_snapshot.sessionPollCount++;
    Gh3018GoodixHrSpo2_ForceWearForGreenValidation();
    g_gh3018_goodix_hrspo2_snapshot.intStatus = HBD_GetIntStatus();
    Gh3018GoodixHrSpo2_CountInterrupt(
        g_gh3018_goodix_hrspo2_snapshot.intStatus);
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();

    if (!Gh3018GoodixHrSpo2_IntHasData(
            g_gh3018_goodix_hrspo2_snapshot.intStatus)) {
        g_gh3018_goodix_hrspo2_snapshot.noDataCount++;
        g_gh3018_goodix_hrspo2_snapshot.sessionNoDataCount++;
        if ((s_greenRawFallbackActive == 0U) &&
            (g_gh3018_goodix_hrspo2_snapshot.sessionNoDataCount >=
             GH3018_GOODIX_HRSPO2_NO_DATA_FALLBACK_THRESHOLD)) {
            Gh3018GoodixHrSpo2_StartGreenRawFallback(now);
        } else if (s_greenRawFallbackActive != 0U) {
            s_greenRawFallbackNoDataStreak++;
            if (s_greenRawFallbackNoDataStreak >=
                GH3018_GOODIX_HRSPO2_GREEN_NO_DATA_RESTART_THRESHOLD) {
                Gh3018GoodixHrSpo2_StartGreenRawFallback(now);
            }
            if (s_greenRawFallbackActive != 0U) {
                g_gh3018_goodix_hrspo2_snapshot.status =
                    GH3018_GOODIX_HRSPO2_STATUS_GREEN_RAW_FALLBACK;
            }
        } else if (g_gh3018_goodix_hrspo2_snapshot.status !=
                   GH3018_GOODIX_HRSPO2_STATUS_WAITING_DATA) {
            g_gh3018_goodix_hrspo2_snapshot.status =
                GH3018_GOODIX_HRSPO2_STATUS_RUNNING;
        }
        Gh3018GoodixHrSpo2_CopyPpgResult(now);
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    memset(s_rawData, 0, sizeof(s_rawData));
    memset(&s_hbResult, 0, sizeof(s_hbResult));
    memset(&s_hrvResult, 0, sizeof(s_hrvResult));
    s_greenRawFallbackNoDataStreak = 0U;
    if (s_greenRawFallbackActive != 0U) {
        rawRet = HBD_GetRawdataByNewDataInt(&rawPpg0, &rawPpg1);
        if (rawRet == HBD_RET_OK) {
            s_rawData[0][0] = (GS32)rawPpg0;
            s_rawData[0][1] = (GS32)rawPpg1;
            g_gh3018_goodix_hrspo2_snapshot.rawDataLen = 1U;
            if (Gh3018GoodixHrSpo2_FilterPpgRaw(
                    (int32_t)rawPpg0,
                    now) != 0U) {
                (void)Gh3018GreenPpgBpm_PushSampleAt(
                    (int32_t)rawPpg0,
                    now);
            }
        } else {
            g_gh3018_goodix_hrspo2_snapshot.rawDataLen = 0U;
        }
        g_gh3018_goodix_hrspo2_snapshot.hbdCalcRet =
            HBD_HbCalculateByNewdataIntDbgEx(
                NULL,
                HBD_GSENSOR_SENSITIVITY_1024_COUNTS_PER_G,
                &s_hbResult);
        HBD_GetRawdataHasDone();
    } else {
        g_gh3018_goodix_hrspo2_snapshot.rawDataLen =
            GH3018_GOODIX_HRSPO2_RAW_FRAME_CAPACITY;
        g_gh3018_goodix_hrspo2_snapshot.hbdCalcRet =
            HBD_HbWithHrvCalculateByFifoIntDbgDataEx(
                NULL,
                0U,
                HBD_GSENSOR_SENSITIVITY_1024_COUNTS_PER_G,
                &s_hbResult,
                &s_hrvResult,
                s_rawData,
                &g_gh3018_goodix_hrspo2_snapshot.rawDataLen);
    }
    g_gh3018_goodix_hrspo2_snapshot.calcCount++;
    g_gh3018_goodix_hrspo2_snapshot.sessionCalcCount++;
    g_gh3018_goodix_hrspo2_snapshot.lastUpdateTick = now;
    Gh3018GoodixHrSpo2_UpdateRawDiagnostics(
        g_gh3018_goodix_hrspo2_snapshot.rawDataLen);
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();

    if ((g_gh3018_goodix_hrspo2_snapshot.hbdCalcRet < 0) &&
        (s_greenRawFallbackActive == 0U)) {
        g_gh3018_goodix_hrspo2_snapshot.status =
            GH3018_GOODIX_HRSPO2_STATUS_CALC_FAIL;
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    if (g_gh3018_goodix_hrspo2_snapshot.hbdCalcRet >= 0) {
        Gh3018GoodixHrSpo2_CopyGoodixResult(now);
    }
    Gh3018GoodixHrSpo2_CopyPpgResult(now);
    if (g_gh3018_goodix_hrspo2_snapshot.hbdCalcRet > 0) {
        g_gh3018_goodix_hrspo2_snapshot.resultRefreshCount++;
        g_gh3018_goodix_hrspo2_snapshot.sessionResultRefreshCount++;
        g_gh3018_goodix_hrspo2_snapshot.status =
            GH3018_GOODIX_HRSPO2_STATUS_RESULT_REFRESHED;
        return &g_gh3018_goodix_hrspo2_snapshot;
    }

    g_gh3018_goodix_hrspo2_snapshot.status =
        (s_greenRawFallbackActive != 0U) ?
            GH3018_GOODIX_HRSPO2_STATUS_GREEN_RAW_FALLBACK :
            GH3018_GOODIX_HRSPO2_STATUS_RUNNING;
    return &g_gh3018_goodix_hrspo2_snapshot;
}

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_BeginWearCheck(void)
{
    uint32_t now = HAL_GetTick();

    s_ppgWearHighStartTick = 0U;
    s_ppgWearLowStartTick = 0U;
    s_ppgWearHighActive = 0U;
    s_ppgWearLowActive = 0U;
    s_ppgWearStateChangeTick = now;
    g_gh3018_goodix_hrspo2_snapshot.ppgWearStableMs = 0U;
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    return &g_gh3018_goodix_hrspo2_snapshot;
}

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_ResetPpgBpm(void)
{
    Gh3018GreenPpgBpm_Reset();
    s_ppgLastCandidateTickMs = 0U;
    s_ppgLastIntervalTickMs = 0U;
    s_ppgLastCandidatePeakCount = 0U;
    s_ppgLastIntervalAcceptedCount = 0U;
    s_ppgHasLastCandidateTick = 0U;
    s_ppgHasLastIntervalTick = 0U;
    s_ppgIntervalExpiryActive = 0U;

    Gh3018GoodixHrSpo2_ClearPpgBpm(
        GH3018_GOODIX_HRSPO2_BPM_GATE_PPG_INVALID);
    g_gh3018_goodix_hrspo2_snapshot.ppgSampleCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAcRange = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgLastIntervalMs = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgCandidatePeakCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalAcceptedCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalTooShortCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalTooLongCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalRejectedOutlierCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgLastRejectedIntervalMs = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakThreshold = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgSignalQuality = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBpm = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrScore = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrValid = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBestScore = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrSecondScore = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrRejectReason = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgMotionFreeze = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgDisplayedBpm = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgPendingBpm = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrLag = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrBestLag = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgAutoCorrSecondLag = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgDcDrift1s = 0;
    g_gh3018_goodix_hrspo2_snapshot.ppgPeakAgeMs =
        GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.ppgCandidateAgeMs =
        GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalAgeMs =
        GH3018_GOODIX_HRSPO2_WEAR_AGE_INVALID;
    g_gh3018_goodix_hrspo2_snapshot.ppgIntervalExpiredCount = 0U;
    g_gh3018_goodix_hrspo2_snapshot.ppgPendingPeakExpiredCount = 0U;
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    return &g_gh3018_goodix_hrspo2_snapshot;
}

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_SetManualWear(
    uint8_t enabled)
{
    uint32_t now = HAL_GetTick();

    if (enabled != 0U) {
        if (s_manualWearOverride == 0U) {
            s_manualWearStartTick = now;
        }
        s_manualWearOverride = 1U;
        Gh3018GoodixHrSpo2_SetPpgWearState(1U, now);
        Gh3018GoodixHrSpo2_ApplyManualWearOverride(now);
        Gh3018GoodixHrSpo2_UpdateSpo2Sim(now);
    } else {
        s_manualWearOverride = 0U;
        s_manualWearStartTick = 0U;
        Gh3018GoodixHrSpo2_SetPpgWearState(0U, now);
        Gh3018GoodixHrSpo2_ClearPpgBpm(
            GH3018_GOODIX_HRSPO2_BPM_GATE_WEAR_OFF);
        Gh3018GoodixHrSpo2_ClearSpo2Sim();
    }

    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    return &g_gh3018_goodix_hrspo2_snapshot;
}

const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2_GetSnapshot(void)
{
    Gh3018GoodixHrSpo2_UpdateRuntimeSnapshot();
    return &g_gh3018_goodix_hrspo2_snapshot;
}

const char *Gh3018GoodixHrSpo2_StatusName(
    Gh3018GoodixHrSpo2Status status)
{
    switch (status) {
    case GH3018_GOODIX_HRSPO2_STATUS_IDLE:
        return "IDLE";
    case GH3018_GOODIX_HRSPO2_STATUS_COMM_FAIL:
        return "COMM_FAIL";
    case GH3018_GOODIX_HRSPO2_STATUS_SIMPLE_INIT_FAIL:
        return "SIMPLE_INIT_FAIL";
    case GH3018_GOODIX_HRSPO2_STATUS_READY:
        return "READY";
    case GH3018_GOODIX_HRSPO2_STATUS_START_FAIL:
        return "START_FAIL";
    case GH3018_GOODIX_HRSPO2_STATUS_LED_SET_FAIL:
        return "LED_SET_FAIL";
    case GH3018_GOODIX_HRSPO2_STATUS_WAITING_DATA:
        return "WAITING_DATA";
    case GH3018_GOODIX_HRSPO2_STATUS_RUNNING:
        return "RUNNING";
    case GH3018_GOODIX_HRSPO2_STATUS_RESULT_REFRESHED:
        return "RESULT_REFRESHED";
    case GH3018_GOODIX_HRSPO2_STATUS_CALC_FAIL:
        return "CALC_FAIL";
    case GH3018_GOODIX_HRSPO2_STATUS_GREEN_RAW_FALLBACK:
        return "GREEN_RAW_FALLBACK";
    case GH3018_GOODIX_HRSPO2_STATUS_STOPPED:
        return "STOPPED";
    case GH3018_GOODIX_HRSPO2_STATUS_STOP_FAIL:
        return "STOP_FAIL";
    default:
        return "UNKNOWN";
    }
}
