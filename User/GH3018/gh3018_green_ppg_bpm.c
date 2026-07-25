#include "gh3018_green_ppg_bpm.h"

#include <string.h>

#define GH3018_GREEN_PPG_SAMPLE_RATE_HZ 25U
#define GH3018_GREEN_PPG_SAMPLE_PERIOD_MS \
    (1000U / GH3018_GREEN_PPG_SAMPLE_RATE_HZ)
#define GH3018_GREEN_PPG_WINDOW_SAMPLES 125U
#define GH3018_GREEN_PPG_MIN_VALID_SAMPLES 75U
#define GH3018_GREEN_PPG_MIN_RUN_MS 3000U
#define GH3018_GREEN_PPG_INTERVAL_HISTORY 4U
#define GH3018_GREEN_PPG_MIN_BPM 40U
#define GH3018_GREEN_PPG_MAX_BPM 150U
#define GH3018_GREEN_PPG_MIN_INTERVAL_MS 400U
#define GH3018_GREEN_PPG_MAX_INTERVAL_MS 1500U
#define GH3018_GREEN_PPG_LAG_SCALE \
    (GH3018_GREEN_PPG_SAMPLE_RATE_HZ * 60U)
#define GH3018_GREEN_PPG_MIN_LAG \
    (GH3018_GREEN_PPG_LAG_SCALE / GH3018_GREEN_PPG_MAX_BPM)
#define GH3018_GREEN_PPG_MAX_LAG \
    (GH3018_GREEN_PPG_LAG_SCALE / GH3018_GREEN_PPG_MIN_BPM)
#define GH3018_GREEN_PPG_AUTOCORR_MIN_SCORE 35U
#define GH3018_GREEN_PPG_MIN_CONFIDENCE 35U
#define GH3018_GREEN_PPG_MIN_AC_RANGE 16U
#define GH3018_GREEN_PPG_MIN_PEAK_AC 6
#define GH3018_GREEN_PPG_MAX_PEAK_AC 800
#define GH3018_GREEN_PPG_PEAK_THRESHOLD_DIVISOR 8U
#define GH3018_GREEN_PPG_MIN_VALID_INTERVALS 2U
#define GH3018_GREEN_PPG_OUTLIER_LOW_PERCENT 70U
#define GH3018_GREEN_PPG_PENDING_PEAK_TIMEOUT_MS 2000U
#define GH3018_GREEN_PPG_BASELINE_SETTLE_MS 1200U
#define GH3018_GREEN_PPG_BASELINE_JUMP_MIN_AC 2000U
#define GH3018_GREEN_PPG_BASELINE_JUMP_RANGE_MULTIPLIER 4U
#define GH3018_GREEN_PPG_SHORT_CLUSTER_WINDOW_MS 2000U
#define GH3018_GREEN_PPG_SHORT_CLUSTER_LIMIT 3U
#define GH3018_GREEN_PPG_PEAK_SUPPRESS_MS 800U
#define GH3018_GREEN_PPG_DC_DRIFT_SAMPLES GH3018_GREEN_PPG_SAMPLE_RATE_HZ
#define GH3018_GREEN_PPG_DC_DRIFT_DIVISOR 100U
#define GH3018_GREEN_PPG_FREEZE_RELEASE_MS 1200U
#define GH3018_GREEN_PPG_BPM_JUMP_THRESHOLD 12U
#define GH3018_GREEN_PPG_BPM_PENDING_CONFIRM 2U

typedef struct {
    uint8_t bpm;
    uint8_t score;
    uint8_t validCandidate;
    uint8_t bestScore;
    uint8_t secondScore;
    uint8_t rejectReason;
    uint16_t lag;
    uint16_t bestLag;
    uint16_t secondLag;
} Gh3018GreenPpgAutoCorrResult;

typedef struct {
    Gh3018GreenPpgBpmSnapshot snapshot;
    int32_t dc;
    int32_t prevAc;
    int32_t prevSlope;
    int32_t acWindow[GH3018_GREEN_PPG_WINDOW_SAMPLES];
    int32_t dcWindow[GH3018_GREEN_PPG_DC_DRIFT_SAMPLES];
    uint16_t intervalMs[GH3018_GREEN_PPG_INTERVAL_HISTORY];
    uint32_t firstSampleTickMs;
    uint32_t prevSampleTickMs;
    uint32_t lastPeakTickMs;
    uint32_t settleUntilTickMs;
    uint32_t suppressUntilTickMs;
    uint32_t shortClusterStartTickMs;
    uint32_t lastFreezeTickMs;
    int32_t lastPeakAc;
    int32_t localValleyAc;
    uint16_t windowIndex;
    uint16_t windowCount;
    uint8_t intervalIndex;
    uint8_t intervalCount;
    uint8_t dcWindowIndex;
    uint8_t dcWindowCount;
    uint8_t shortClusterCount;
    uint8_t pendingBpmConfirmCount;
    uint8_t hasFirstSampleTick;
    uint8_t hasPrevAc;
    uint8_t hasPrevSlope;
    uint8_t hasPendingPeak;
    uint8_t hasLocalValley;
    uint8_t settlingActive;
    uint8_t hasDisplayedBpm;
    uint8_t hasPendingBpm;
} Gh3018GreenPpgBpmState;

static Gh3018GreenPpgBpmState s_ppg;

static uint8_t Gh3018GreenPpgBpm_ClampU8(uint32_t value)
{
    return (value > 100U) ? 100U : (uint8_t)value;
}

static uint32_t Gh3018GreenPpgBpm_Abs32(int32_t value)
{
    if (value < 0) {
        return (uint32_t)(-value);
    }
    return (uint32_t)value;
}

static uint32_t Gh3018GreenPpgBpm_MaxU32(uint32_t a, uint32_t b)
{
    return (a > b) ? a : b;
}

static uint32_t Gh3018GreenPpgBpm_MinU32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static int32_t Gh3018GreenPpgBpm_ClampI32(
    int32_t value,
    int32_t minValue,
    int32_t maxValue)
{
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static uint8_t Gh3018GreenPpgBpm_BpmDelta(uint8_t a, uint8_t b)
{
    return (a > b) ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

static uint8_t Gh3018GreenPpgBpm_TickBefore(uint32_t now, uint32_t target)
{
    return ((int32_t)(now - target) < 0) ? 1U : 0U;
}

static void Gh3018GreenPpgBpm_ResetPeakTracker(
    int32_t ac,
    uint32_t sampleTickMs)
{
    s_ppg.prevAc = ac;
    s_ppg.prevSampleTickMs = sampleTickMs;
    s_ppg.lastPeakTickMs = 0U;
    s_ppg.lastPeakAc = 0;
    s_ppg.localValleyAc = ac;
    s_ppg.hasPrevAc = 1U;
    s_ppg.hasPrevSlope = 0U;
    s_ppg.hasPendingPeak = 0U;
    s_ppg.hasLocalValley = 1U;
}

static uint32_t Gh3018GreenPpgBpm_CalcRange(void)
{
    int32_t minAc = 0;
    int32_t maxAc = 0;

    if (s_ppg.windowCount == 0U) {
        return 0U;
    }

    minAc = s_ppg.acWindow[0];
    maxAc = s_ppg.acWindow[0];
    for (uint16_t i = 1U; i < s_ppg.windowCount; ++i) {
        if (s_ppg.acWindow[i] < minAc) {
            minAc = s_ppg.acWindow[i];
        }
        if (s_ppg.acWindow[i] > maxAc) {
            maxAc = s_ppg.acWindow[i];
        }
    }

    return (uint32_t)(maxAc - minAc);
}

static int32_t Gh3018GreenPpgBpm_GetWindowAc(uint16_t chronologicalIndex)
{
    uint16_t baseIndex = 0U;
    uint16_t physicalIndex;

    if (s_ppg.windowCount >= GH3018_GREEN_PPG_WINDOW_SAMPLES) {
        baseIndex = s_ppg.windowIndex;
    }
    physicalIndex = (uint16_t)((baseIndex + chronologicalIndex) %
                               GH3018_GREEN_PPG_WINDOW_SAMPLES);
    return s_ppg.acWindow[physicalIndex];
}

static void Gh3018GreenPpgBpm_ClearAcWindow(void)
{
    memset(s_ppg.acWindow, 0, sizeof(s_ppg.acWindow));
    s_ppg.windowIndex = 0U;
    s_ppg.windowCount = 0U;
    s_ppg.snapshot.acRange = 0U;
    s_ppg.snapshot.peakThreshold = GH3018_GREEN_PPG_MIN_PEAK_AC;
}

static void Gh3018GreenPpgBpm_SeedAcWindow(int32_t ac)
{
    s_ppg.acWindow[0] = ac;
    s_ppg.windowIndex = 1U;
    s_ppg.windowCount = 1U;
    s_ppg.snapshot.acRange = 0U;
}

static uint8_t Gh3018GreenPpgBpm_RangeScore(uint32_t acRange)
{
    uint32_t score;

    if (acRange == 0U) {
        return 0U;
    }
    if (acRange < GH3018_GREEN_PPG_MIN_AC_RANGE) {
        return Gh3018GreenPpgBpm_ClampU8(
            (acRange * 30U) / GH3018_GREEN_PPG_MIN_AC_RANGE);
    }

    score = 30U +
        (((acRange - GH3018_GREEN_PPG_MIN_AC_RANGE) * 70U) /
         (GH3018_GREEN_PPG_MIN_AC_RANGE * 8U));
    return Gh3018GreenPpgBpm_ClampU8(score);
}

static void Gh3018GreenPpgBpm_ClearIntervals(void)
{
    memset(s_ppg.intervalMs, 0, sizeof(s_ppg.intervalMs));
    s_ppg.intervalIndex = 0U;
    s_ppg.intervalCount = 0U;
    s_ppg.snapshot.valid = 0U;
    s_ppg.snapshot.confidence = 0U;
    s_ppg.snapshot.bpm = 0U;
    s_ppg.snapshot.lastIntervalMs = 0U;
}

static void Gh3018GreenPpgBpm_ClearPeakHistory(void)
{
    Gh3018GreenPpgBpm_ClearIntervals();
    s_ppg.hasPendingPeak = 0U;
    s_ppg.lastPeakTickMs = 0U;
    s_ppg.lastPeakAc = 0;
    s_ppg.hasPrevSlope = 0U;
    s_ppg.hasLocalValley = 0U;
}

static void Gh3018GreenPpgBpm_EnterBaselineSettle(
    int32_t ac,
    uint32_t sampleTickMs)
{
    Gh3018GreenPpgBpm_ClearPeakHistory();
    Gh3018GreenPpgBpm_ClearAcWindow();
    s_ppg.settleUntilTickMs =
        sampleTickMs + GH3018_GREEN_PPG_BASELINE_SETTLE_MS;
    s_ppg.shortClusterCount = 0U;
    s_ppg.settlingActive = 1U;
    Gh3018GreenPpgBpm_ResetPeakTracker(ac, sampleTickMs);
}

static void Gh3018GreenPpgBpm_FinishBaselineSettle(
    int32_t ac,
    uint32_t sampleTickMs)
{
    Gh3018GreenPpgBpm_ClearPeakHistory();
    Gh3018GreenPpgBpm_ClearAcWindow();
    Gh3018GreenPpgBpm_SeedAcWindow(ac);
    s_ppg.settleUntilTickMs = 0U;
    s_ppg.settlingActive = 0U;
    s_ppg.shortClusterCount = 0U;
    Gh3018GreenPpgBpm_ResetPeakTracker(ac, sampleTickMs);
}

static uint8_t Gh3018GreenPpgBpm_ShouldSettle(
    int32_t ac,
    uint32_t previousAcRange)
{
    uint32_t jumpThreshold;
    uint32_t delta;

    if (s_ppg.hasPrevAc == 0U) {
        return 0U;
    }

    jumpThreshold = Gh3018GreenPpgBpm_MaxU32(
        GH3018_GREEN_PPG_BASELINE_JUMP_MIN_AC,
        previousAcRange * GH3018_GREEN_PPG_BASELINE_JUMP_RANGE_MULTIPLIER);
    delta = (ac > s_ppg.prevAc) ?
        (uint32_t)(ac - s_ppg.prevAc) :
        (uint32_t)(s_ppg.prevAc - ac);

    return ((delta > jumpThreshold) ||
            (Gh3018GreenPpgBpm_Abs32(ac) > jumpThreshold)) ? 1U : 0U;
}

static void Gh3018GreenPpgBpm_AddInterval(uint16_t intervalMs)
{
    s_ppg.intervalMs[s_ppg.intervalIndex] = intervalMs;
    s_ppg.intervalIndex =
        (uint8_t)((s_ppg.intervalIndex + 1U) %
                  GH3018_GREEN_PPG_INTERVAL_HISTORY);
    if (s_ppg.intervalCount < GH3018_GREEN_PPG_INTERVAL_HISTORY) {
        s_ppg.intervalCount++;
    }
    s_ppg.snapshot.intervalAcceptedCount++;
    s_ppg.snapshot.lastIntervalMs = intervalMs;
    s_ppg.shortClusterCount = 0U;
}

static uint16_t Gh3018GreenPpgBpm_MedianIntervalMs(void)
{
    uint16_t sorted[GH3018_GREEN_PPG_INTERVAL_HISTORY];
    uint8_t count = s_ppg.intervalCount;

    if (count == 0U) {
        return 0U;
    }

    for (uint8_t i = 0U; i < count; ++i) {
        sorted[i] = s_ppg.intervalMs[i];
    }

    for (uint8_t i = 0U; i < count; ++i) {
        for (uint8_t j = (uint8_t)(i + 1U); j < count; ++j) {
            if (sorted[j] < sorted[i]) {
                uint16_t tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    if ((count & 1U) != 0U) {
        return sorted[count / 2U];
    }

    return (uint16_t)(((uint32_t)sorted[(count / 2U) - 1U] +
                       sorted[count / 2U] + 1U) /
                      2U);
}

static uint8_t Gh3018GreenPpgBpm_IsShortOutlier(uint16_t intervalMs)
{
    uint16_t medianIntervalMs;

    if (s_ppg.intervalCount < GH3018_GREEN_PPG_MIN_VALID_INTERVALS) {
        return 0U;
    }

    medianIntervalMs = Gh3018GreenPpgBpm_MedianIntervalMs();
    if (medianIntervalMs == 0U) {
        return 0U;
    }

    return ((uint32_t)intervalMs * 100U <
            (uint32_t)medianIntervalMs *
                GH3018_GREEN_PPG_OUTLIER_LOW_PERCENT) ? 1U : 0U;
}

static uint8_t Gh3018GreenPpgBpm_StabilityScore(void)
{
    uint16_t minInterval;
    uint16_t maxInterval;
    uint16_t spread;

    if (s_ppg.intervalCount < 2U) {
        return 30U;
    }

    minInterval = s_ppg.intervalMs[0];
    maxInterval = s_ppg.intervalMs[0];
    for (uint8_t i = 1U; i < s_ppg.intervalCount; ++i) {
        if (s_ppg.intervalMs[i] < minInterval) {
            minInterval = s_ppg.intervalMs[i];
        }
        if (s_ppg.intervalMs[i] > maxInterval) {
            maxInterval = s_ppg.intervalMs[i];
        }
    }

    spread = (uint16_t)(maxInterval - minInterval);
    if (spread >= 320U) {
        return 20U;
    }
    return (uint8_t)(100U - ((spread * 80U) / 320U));
}

static uint8_t Gh3018GreenPpgBpm_RecordTooShortPeak(
    uint32_t candidateTickMs)
{
    if ((s_ppg.shortClusterCount == 0U) ||
        ((candidateTickMs - s_ppg.shortClusterStartTickMs) >
         GH3018_GREEN_PPG_SHORT_CLUSTER_WINDOW_MS)) {
        s_ppg.shortClusterStartTickMs = candidateTickMs;
        s_ppg.shortClusterCount = 1U;
    } else {
        s_ppg.shortClusterCount++;
    }

    if (s_ppg.shortClusterCount < GH3018_GREEN_PPG_SHORT_CLUSTER_LIMIT) {
        return 0U;
    }

    Gh3018GreenPpgBpm_ClearPeakHistory();
    s_ppg.suppressUntilTickMs =
        candidateTickMs + GH3018_GREEN_PPG_PEAK_SUPPRESS_MS;
    s_ppg.shortClusterCount = 0U;
    return 1U;
}

static void Gh3018GreenPpgBpm_UpdateMotionFreeze(
    int32_t dc,
    uint32_t sampleTickMs)
{
    uint32_t driftThreshold;
    uint32_t drift = 0U;

    if (s_ppg.dcWindowCount >= GH3018_GREEN_PPG_DC_DRIFT_SAMPLES) {
        int32_t dc1sAgo = s_ppg.dcWindow[s_ppg.dcWindowIndex];
        drift = (dc > dc1sAgo) ?
            (uint32_t)(dc - dc1sAgo) :
            (uint32_t)(dc1sAgo - dc);
        driftThreshold =
            Gh3018GreenPpgBpm_MaxU32(
                Gh3018GreenPpgBpm_Abs32(dc) /
                    GH3018_GREEN_PPG_DC_DRIFT_DIVISOR,
                1U);
        if (drift > driftThreshold) {
            s_ppg.snapshot.motionFreeze = 1U;
            s_ppg.lastFreezeTickMs = sampleTickMs;
        } else if ((s_ppg.snapshot.motionFreeze != 0U) &&
                   ((sampleTickMs - s_ppg.lastFreezeTickMs) >=
                    GH3018_GREEN_PPG_FREEZE_RELEASE_MS)) {
            s_ppg.snapshot.motionFreeze = 0U;
        }
    }

    s_ppg.snapshot.dcDrift1s = (int32_t)drift;
    s_ppg.dcWindow[s_ppg.dcWindowIndex] = dc;
    s_ppg.dcWindowIndex =
        (uint8_t)((s_ppg.dcWindowIndex + 1U) %
                  GH3018_GREEN_PPG_DC_DRIFT_SAMPLES);
    if (s_ppg.dcWindowCount < GH3018_GREEN_PPG_DC_DRIFT_SAMPLES) {
        s_ppg.dcWindowCount++;
    }
}

static uint8_t Gh3018GreenPpgBpm_CalcAutoCorr(
    Gh3018GreenPpgAutoCorrResult *result)
{
    int64_t sumAc = 0;
    int64_t mean;
    uint32_t bestScore = 0U;
    uint32_t secondScore = 0U;
    uint16_t bestLag = 0U;
    uint16_t secondLag = 0U;
    uint16_t chosenLag = 0U;
    uint8_t chosenScore = 0U;
    uint16_t sampleCount;

    if (result == NULL) {
        return 0U;
    }
    memset(result, 0, sizeof(*result));
    result->rejectReason =
        GH3018_GREEN_PPG_AUTOCORR_REJECT_NOT_READY;

    sampleCount = s_ppg.windowCount;
    if (sampleCount < GH3018_GREEN_PPG_MIN_VALID_SAMPLES) {
        return 0U;
    }

    for (uint16_t i = 0U; i < sampleCount; ++i) {
        sumAc += Gh3018GreenPpgBpm_GetWindowAc(i);
    }
    mean = sumAc / sampleCount;

    for (uint16_t testLag = GH3018_GREEN_PPG_MIN_LAG;
         testLag <= GH3018_GREEN_PPG_MAX_LAG;
         ++testLag) {
        int64_t corr = 0;
        int64_t energyA = 0;
        int64_t energyB = 0;
        int64_t denom;
        uint32_t lagScore;

        for (uint16_t i = testLag; i < sampleCount; ++i) {
            int64_t a =
                (int64_t)Gh3018GreenPpgBpm_GetWindowAc(i) - mean;
            int64_t b =
                (int64_t)Gh3018GreenPpgBpm_GetWindowAc(
                    (uint16_t)(i - testLag)) - mean;
            corr += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        if ((corr <= 0) || (energyA <= 0) || (energyB <= 0)) {
            lagScore = 0U;
        } else {
            denom = (energyA + energyB) / 2;
            if (denom <= 0) {
                lagScore = 0U;
            } else {
                int64_t scaled =
                    ((corr * 100) + (denom / 2)) / denom;
                if (scaled > 100) {
                    lagScore = 100U;
                } else {
                    lagScore = (uint32_t)scaled;
                }
            }
        }

        if (lagScore > bestScore) {
            bestScore = lagScore;
            bestLag = testLag;
        }
        if ((testLag > GH3018_GREEN_PPG_MIN_LAG) &&
            (testLag < GH3018_GREEN_PPG_MAX_LAG) &&
            (lagScore > secondScore)) {
            secondScore = lagScore;
            secondLag = testLag;
        }
    }

    if (bestLag == 0U) {
        result->rejectReason =
            GH3018_GREEN_PPG_AUTOCORR_REJECT_LOW_SCORE;
        return 0U;
    }

    result->bestLag = bestLag;
    result->bestScore = Gh3018GreenPpgBpm_ClampU8(bestScore);
    result->secondLag = secondLag;
    result->secondScore = Gh3018GreenPpgBpm_ClampU8(secondScore);

    if ((bestLag > GH3018_GREEN_PPG_MIN_LAG) &&
        (bestLag < GH3018_GREEN_PPG_MAX_LAG)) {
        chosenLag = bestLag;
        chosenScore = result->bestScore;
    } else if ((secondLag != 0U) &&
               (result->secondScore >=
                GH3018_GREEN_PPG_AUTOCORR_MIN_SCORE)) {
        chosenLag = secondLag;
        chosenScore = result->secondScore;
    } else {
        result->rejectReason =
            GH3018_GREEN_PPG_AUTOCORR_REJECT_EDGE;
        return 1U;
    }

    result->lag = chosenLag;
    result->score = chosenScore;
    result->bpm = (uint8_t)((GH3018_GREEN_PPG_LAG_SCALE +
                             (chosenLag / 2U)) / chosenLag);
    if (chosenScore >= GH3018_GREEN_PPG_AUTOCORR_MIN_SCORE) {
        result->validCandidate = 1U;
        result->rejectReason = GH3018_GREEN_PPG_AUTOCORR_REJECT_NONE;
    } else {
        result->rejectReason =
            GH3018_GREEN_PPG_AUTOCORR_REJECT_LOW_SCORE;
    }
    return 1U;
}

static uint8_t Gh3018GreenPpgBpm_ApplyBpmSmoothing(uint8_t autoCorrBpm)
{
    uint8_t delta;

    if (s_ppg.hasDisplayedBpm == 0U) {
        s_ppg.snapshot.displayedBpm = autoCorrBpm;
        s_ppg.hasDisplayedBpm = 1U;
        s_ppg.hasPendingBpm = 0U;
        s_ppg.pendingBpmConfirmCount = 0U;
        s_ppg.snapshot.pendingBpm = 0U;
        return s_ppg.snapshot.displayedBpm;
    }

    delta = Gh3018GreenPpgBpm_BpmDelta(
        autoCorrBpm,
        s_ppg.snapshot.displayedBpm);
    if (delta <= GH3018_GREEN_PPG_BPM_JUMP_THRESHOLD) {
        s_ppg.snapshot.displayedBpm =
            (uint8_t)(((uint32_t)s_ppg.snapshot.displayedBpm * 8U +
                       (uint32_t)autoCorrBpm * 2U + 5U) / 10U);
        s_ppg.hasPendingBpm = 0U;
        s_ppg.pendingBpmConfirmCount = 0U;
        s_ppg.snapshot.pendingBpm = 0U;
        return s_ppg.snapshot.displayedBpm;
    }

    if ((s_ppg.hasPendingBpm == 0U) ||
        (Gh3018GreenPpgBpm_BpmDelta(
             autoCorrBpm,
             s_ppg.snapshot.pendingBpm) >
         GH3018_GREEN_PPG_BPM_JUMP_THRESHOLD)) {
        s_ppg.snapshot.pendingBpm = autoCorrBpm;
        s_ppg.pendingBpmConfirmCount = 1U;
        s_ppg.hasPendingBpm = 1U;
        return s_ppg.snapshot.displayedBpm;
    }

    s_ppg.pendingBpmConfirmCount++;
    s_ppg.snapshot.pendingBpm =
        (uint8_t)(((uint32_t)s_ppg.snapshot.pendingBpm +
                   autoCorrBpm + 1U) / 2U);
    if (s_ppg.pendingBpmConfirmCount >=
        GH3018_GREEN_PPG_BPM_PENDING_CONFIRM) {
        s_ppg.snapshot.displayedBpm =
            (uint8_t)(((uint32_t)s_ppg.snapshot.displayedBpm * 8U +
                       (uint32_t)s_ppg.snapshot.pendingBpm * 2U + 5U) /
                      10U);
        s_ppg.hasPendingBpm = 0U;
        s_ppg.pendingBpmConfirmCount = 0U;
        s_ppg.snapshot.pendingBpm = 0U;
    }

    return s_ppg.snapshot.displayedBpm;
}

static void Gh3018GreenPpgBpm_UpdateResult(uint32_t sampleTickMs)
{
    uint32_t runMs = 0U;
    uint16_t medianIntervalMs;
    uint8_t peakBpm = 0U;
    uint8_t peakValid;
    uint8_t rangeScore = Gh3018GreenPpgBpm_RangeScore(
        s_ppg.snapshot.acRange);
    uint8_t peakScore =
        Gh3018GreenPpgBpm_ClampU8(20U + (s_ppg.intervalCount * 35U));
    uint8_t stabilityScore = Gh3018GreenPpgBpm_StabilityScore();
    uint8_t maturityScore =
        Gh3018GreenPpgBpm_ClampU8(
            (Gh3018GreenPpgBpm_MinU32(
                 s_ppg.snapshot.sampleCount,
                 GH3018_GREEN_PPG_MIN_VALID_SAMPLES) * 100U) /
            GH3018_GREEN_PPG_MIN_VALID_SAMPLES);
    Gh3018GreenPpgAutoCorrResult autoCorr;
    uint8_t hasAutoCorr;
    uint32_t confidence;
    uint8_t autoCorrValid;

    hasAutoCorr = Gh3018GreenPpgBpm_CalcAutoCorr(
        &autoCorr);
    s_ppg.snapshot.autoCorrBpm = autoCorr.bpm;
    s_ppg.snapshot.autoCorrScore = autoCorr.score;
    s_ppg.snapshot.autoCorrLag = autoCorr.lag;
    s_ppg.snapshot.autoCorrBestScore = autoCorr.bestScore;
    s_ppg.snapshot.autoCorrBestLag = autoCorr.bestLag;
    s_ppg.snapshot.autoCorrSecondScore = autoCorr.secondScore;
    s_ppg.snapshot.autoCorrSecondLag = autoCorr.secondLag;
    s_ppg.snapshot.autoCorrRejectReason = autoCorr.rejectReason;

    s_ppg.snapshot.quality = Gh3018GreenPpgBpm_ClampU8(
        ((uint32_t)rangeScore * 35U +
         (uint32_t)peakScore * 30U +
         (uint32_t)stabilityScore * 25U +
         (uint32_t)maturityScore * 10U) / 100U);
    if (s_ppg.snapshot.sampleCount < GH3018_GREEN_PPG_MIN_VALID_SAMPLES) {
        s_ppg.snapshot.quality = Gh3018GreenPpgBpm_ClampU8(
            ((uint32_t)s_ppg.snapshot.quality * maturityScore) / 100U);
    }

    if (s_ppg.hasFirstSampleTick != 0U) {
        runMs = sampleTickMs - s_ppg.firstSampleTickMs;
    }
    autoCorrValid =
        (hasAutoCorr != 0U) &&
        (autoCorr.validCandidate != 0U) &&
        (s_ppg.snapshot.sampleCount >= GH3018_GREEN_PPG_MIN_VALID_SAMPLES) &&
        (s_ppg.windowCount >= GH3018_GREEN_PPG_MIN_VALID_SAMPLES) &&
        (runMs >= GH3018_GREEN_PPG_MIN_RUN_MS) &&
        (s_ppg.snapshot.acRange >= GH3018_GREEN_PPG_MIN_AC_RANGE) &&
        (autoCorr.score >= GH3018_GREEN_PPG_AUTOCORR_MIN_SCORE) &&
        (autoCorr.lag > GH3018_GREEN_PPG_MIN_LAG) &&
        (autoCorr.lag < GH3018_GREEN_PPG_MAX_LAG) &&
        (autoCorr.bpm >= GH3018_GREEN_PPG_MIN_BPM) &&
        (autoCorr.bpm <= GH3018_GREEN_PPG_MAX_BPM) &&
        (s_ppg.snapshot.motionFreeze == 0U) ? 1U : 0U;
    s_ppg.snapshot.autoCorrValid = autoCorrValid;

    if (s_ppg.snapshot.motionFreeze != 0U) {
        s_ppg.snapshot.autoCorrRejectReason =
            GH3018_GREEN_PPG_AUTOCORR_REJECT_MOTION_FREEZE;
    }

    medianIntervalMs = Gh3018GreenPpgBpm_MedianIntervalMs();
    if (medianIntervalMs != 0U) {
        peakBpm = (uint8_t)((60000U + (medianIntervalMs / 2U)) /
                            medianIntervalMs);
    }
    peakValid =
        (medianIntervalMs != 0U) &&
        (s_ppg.intervalCount >= GH3018_GREEN_PPG_MIN_VALID_INTERVALS) &&
        (peakBpm >= GH3018_GREEN_PPG_MIN_BPM) &&
        (peakBpm <= GH3018_GREEN_PPG_MAX_BPM) ? 1U : 0U;

    if (peakValid == 0U) {
        s_ppg.snapshot.valid = 0U;
        s_ppg.snapshot.confidence = 0U;
        s_ppg.snapshot.bpm = 0U;
        return;
    }

    confidence =
        ((uint32_t)peakScore * 35U +
         (uint32_t)rangeScore * 30U +
         (uint32_t)stabilityScore * 25U +
         (uint32_t)maturityScore * 10U) / 100U;

    s_ppg.snapshot.bpm =
        Gh3018GreenPpgBpm_ApplyBpmSmoothing(peakBpm);
    s_ppg.snapshot.confidence = Gh3018GreenPpgBpm_ClampU8(confidence);
    s_ppg.snapshot.valid = 1U;
}

static void Gh3018GreenPpgBpm_CheckPeak(int32_t ac, uint32_t sampleTickMs)
{
    int32_t slope;
    int32_t threshold;
    uint32_t candidateTickMs;
    uint32_t intervalMs;
    uint8_t candidateHandled = 0U;

    if (s_ppg.hasPrevAc == 0U) {
        s_ppg.prevAc = ac;
        s_ppg.prevSampleTickMs = sampleTickMs;
        s_ppg.localValleyAc = ac;
        s_ppg.hasPrevAc = 1U;
        s_ppg.hasLocalValley = 1U;
        return;
    }

    slope = ac - s_ppg.prevAc;
    if (s_ppg.hasPrevSlope != 0U) {
        threshold = Gh3018GreenPpgBpm_ClampI32(
            (int32_t)(s_ppg.snapshot.acRange /
                      GH3018_GREEN_PPG_PEAK_THRESHOLD_DIVISOR),
            GH3018_GREEN_PPG_MIN_PEAK_AC,
            GH3018_GREEN_PPG_MAX_PEAK_AC);
        s_ppg.snapshot.peakThreshold = (uint32_t)threshold;
        if ((s_ppg.prevSlope > 0) && (slope <= 0) &&
            (s_ppg.prevAc >= threshold)) {
            candidateTickMs = s_ppg.prevSampleTickMs;
            candidateHandled = 1U;
            s_ppg.snapshot.candidatePeakCount++;
            if (s_ppg.hasPendingPeak == 0U) {
                s_ppg.lastPeakTickMs = candidateTickMs;
                s_ppg.lastPeakAc = s_ppg.prevAc;
                s_ppg.hasPendingPeak = 1U;
                s_ppg.snapshot.peakCount++;
            } else {
                intervalMs = candidateTickMs - s_ppg.lastPeakTickMs;
                if (intervalMs >
                    GH3018_GREEN_PPG_PENDING_PEAK_TIMEOUT_MS) {
                    s_ppg.snapshot.pendingPeakExpiredCount++;
                    Gh3018GreenPpgBpm_ClearIntervals();
                    s_ppg.shortClusterCount = 0U;
                    s_ppg.lastPeakTickMs = candidateTickMs;
                    s_ppg.lastPeakAc = s_ppg.prevAc;
                    s_ppg.snapshot.peakCount++;
                } else if (intervalMs <
                           GH3018_GREEN_PPG_MIN_INTERVAL_MS) {
                    s_ppg.snapshot.intervalTooShortCount++;
                    s_ppg.snapshot.lastRejectedIntervalMs =
                        (uint16_t)intervalMs;
                    if ((Gh3018GreenPpgBpm_RecordTooShortPeak(
                            candidateTickMs) == 0U) &&
                        (s_ppg.prevAc > s_ppg.lastPeakAc)) {
                        s_ppg.lastPeakTickMs = candidateTickMs;
                        s_ppg.lastPeakAc = s_ppg.prevAc;
                    }
                } else if (intervalMs <=
                           GH3018_GREEN_PPG_MAX_INTERVAL_MS) {
                    if (Gh3018GreenPpgBpm_IsShortOutlier(
                            (uint16_t)intervalMs) != 0U) {
                        s_ppg.snapshot.intervalRejectedOutlierCount++;
                        s_ppg.snapshot.lastRejectedIntervalMs =
                            (uint16_t)intervalMs;
                        if (s_ppg.prevAc > s_ppg.lastPeakAc) {
                            s_ppg.lastPeakTickMs = candidateTickMs;
                            s_ppg.lastPeakAc = s_ppg.prevAc;
                        }
                    } else {
                        Gh3018GreenPpgBpm_AddInterval(
                            (uint16_t)intervalMs);
                        s_ppg.lastPeakTickMs = candidateTickMs;
                        s_ppg.lastPeakAc = s_ppg.prevAc;
                        s_ppg.snapshot.peakCount++;
                    }
                } else {
                    s_ppg.snapshot.intervalTooLongCount++;
                    s_ppg.snapshot.lastRejectedIntervalMs =
                        (uint16_t)intervalMs;
                    Gh3018GreenPpgBpm_ClearIntervals();
                    s_ppg.shortClusterCount = 0U;
                    s_ppg.lastPeakTickMs = candidateTickMs;
                    s_ppg.lastPeakAc = s_ppg.prevAc;
                    s_ppg.snapshot.peakCount++;
                }
            }
        }
    }

    if (candidateHandled != 0U) {
        s_ppg.localValleyAc = ac;
        s_ppg.hasLocalValley = 1U;
    } else if ((s_ppg.hasLocalValley == 0U) ||
               (ac < s_ppg.localValleyAc)) {
        s_ppg.localValleyAc = ac;
        s_ppg.hasLocalValley = 1U;
    }
    s_ppg.prevSlope = slope;
    s_ppg.hasPrevSlope = 1U;
    s_ppg.prevAc = ac;
    s_ppg.prevSampleTickMs = sampleTickMs;
}

void Gh3018GreenPpgBpm_Init(void)
{
    Gh3018GreenPpgBpm_Reset();
}

void Gh3018GreenPpgBpm_Reset(void)
{
    memset(&s_ppg, 0, sizeof(s_ppg));
}

void Gh3018GreenPpgBpm_ClearIntervalHistory(void)
{
    Gh3018GreenPpgBpm_ClearPeakHistory();
    s_ppg.shortClusterCount = 0U;
    s_ppg.suppressUntilTickMs = 0U;
}

const Gh3018GreenPpgBpmSnapshot *Gh3018GreenPpgBpm_PushSample(
    int32_t rawPpg)
{
    uint32_t sampleTickMs =
        (s_ppg.hasFirstSampleTick != 0U) ?
            (s_ppg.firstSampleTickMs +
             (s_ppg.snapshot.sampleCount *
              GH3018_GREEN_PPG_SAMPLE_PERIOD_MS)) :
            0U;

    return Gh3018GreenPpgBpm_PushSampleAt(rawPpg, sampleTickMs);
}

const Gh3018GreenPpgBpmSnapshot *Gh3018GreenPpgBpm_PushSampleAt(
    int32_t rawPpg,
    uint32_t sampleTickMs)
{
    int32_t ac;
    uint32_t previousAcRange;

    if (rawPpg <= 0) {
        s_ppg.snapshot.valid = 0U;
        s_ppg.snapshot.confidence = 0U;
        s_ppg.snapshot.quality = 0U;
        s_ppg.snapshot.bpm = 0U;
        s_ppg.snapshot.autoCorrValid = 0U;
        return &s_ppg.snapshot;
    }

    if (s_ppg.hasFirstSampleTick == 0U) {
        s_ppg.firstSampleTickMs = sampleTickMs;
        s_ppg.hasFirstSampleTick = 1U;
    }
    s_ppg.snapshot.sampleCount++;
    if (s_ppg.snapshot.sampleCount == 1U) {
        s_ppg.dc = rawPpg;
    } else {
        s_ppg.dc += (rawPpg - s_ppg.dc) / 16;
    }
    ac = rawPpg - s_ppg.dc;
    previousAcRange = s_ppg.snapshot.acRange;

    s_ppg.acWindow[s_ppg.windowIndex] = ac;
    s_ppg.windowIndex =
        (uint16_t)((s_ppg.windowIndex + 1U) %
                   GH3018_GREEN_PPG_WINDOW_SAMPLES);
    if (s_ppg.windowCount < GH3018_GREEN_PPG_WINDOW_SAMPLES) {
        s_ppg.windowCount++;
    }

    Gh3018GreenPpgBpm_UpdateMotionFreeze(s_ppg.dc, sampleTickMs);
    s_ppg.snapshot.acRange = Gh3018GreenPpgBpm_CalcRange();
    if (Gh3018GreenPpgBpm_TickBefore(
            sampleTickMs,
            s_ppg.settleUntilTickMs) != 0U) {
        Gh3018GreenPpgBpm_ResetPeakTracker(ac, sampleTickMs);
        Gh3018GreenPpgBpm_UpdateResult(sampleTickMs);
        return &s_ppg.snapshot;
    }
    if (s_ppg.settlingActive != 0U) {
        Gh3018GreenPpgBpm_FinishBaselineSettle(ac, sampleTickMs);
        Gh3018GreenPpgBpm_UpdateResult(sampleTickMs);
        return &s_ppg.snapshot;
    }
    if (Gh3018GreenPpgBpm_TickBefore(
            sampleTickMs,
            s_ppg.suppressUntilTickMs) != 0U) {
        Gh3018GreenPpgBpm_ResetPeakTracker(ac, sampleTickMs);
        Gh3018GreenPpgBpm_UpdateResult(sampleTickMs);
        return &s_ppg.snapshot;
    }
    if (Gh3018GreenPpgBpm_ShouldSettle(ac, previousAcRange) != 0U) {
        Gh3018GreenPpgBpm_EnterBaselineSettle(ac, sampleTickMs);
        Gh3018GreenPpgBpm_UpdateResult(sampleTickMs);
        return &s_ppg.snapshot;
    }
    if (Gh3018GreenPpgBpm_Abs32(ac) > 0U) {
        Gh3018GreenPpgBpm_CheckPeak(ac, sampleTickMs);
    }
    Gh3018GreenPpgBpm_UpdateResult(sampleTickMs);

    return &s_ppg.snapshot;
}

const Gh3018GreenPpgBpmSnapshot *Gh3018GreenPpgBpm_GetSnapshot(void)
{
    return &s_ppg.snapshot;
}
