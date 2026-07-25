#ifndef GH3018_GREEN_PPG_BPM_H
#define GH3018_GREEN_PPG_BPM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GH3018_GREEN_PPG_AUTOCORR_REJECT_NONE = 0,
    GH3018_GREEN_PPG_AUTOCORR_REJECT_NOT_READY,
    GH3018_GREEN_PPG_AUTOCORR_REJECT_LOW_SCORE,
    GH3018_GREEN_PPG_AUTOCORR_REJECT_EDGE,
    GH3018_GREEN_PPG_AUTOCORR_REJECT_MOTION_FREEZE
} Gh3018GreenPpgAutoCorrRejectReason;

typedef struct {
    uint8_t bpm;
    uint8_t confidence;
    uint8_t valid;
    uint8_t quality;
    uint8_t autoCorrBpm;
    uint8_t autoCorrScore;
    uint8_t autoCorrValid;
    uint8_t autoCorrBestScore;
    uint8_t autoCorrSecondScore;
    uint8_t autoCorrRejectReason;
    uint8_t motionFreeze;
    uint8_t displayedBpm;
    uint8_t pendingBpm;
    uint16_t autoCorrLag;
    uint16_t autoCorrBestLag;
    uint16_t autoCorrSecondLag;
    int32_t dcDrift1s;
    uint32_t sampleCount;
    uint32_t peakCount;
    uint32_t acRange;
    uint16_t lastIntervalMs;
    uint32_t candidatePeakCount;
    uint32_t intervalAcceptedCount;
    uint32_t intervalTooShortCount;
    uint32_t intervalTooLongCount;
    uint32_t intervalRejectedOutlierCount;
    uint16_t lastRejectedIntervalMs;
    uint32_t peakThreshold;
    uint32_t pendingPeakExpiredCount;
} Gh3018GreenPpgBpmSnapshot;

void Gh3018GreenPpgBpm_Init(void);
void Gh3018GreenPpgBpm_Reset(void);
void Gh3018GreenPpgBpm_ClearIntervalHistory(void);
const Gh3018GreenPpgBpmSnapshot *Gh3018GreenPpgBpm_PushSample(
    int32_t rawPpg);
const Gh3018GreenPpgBpmSnapshot *Gh3018GreenPpgBpm_PushSampleAt(
    int32_t rawPpg,
    uint32_t sampleTickMs);
const Gh3018GreenPpgBpmSnapshot *Gh3018GreenPpgBpm_GetSnapshot(void);

#ifdef __cplusplus
}
#endif

#endif
