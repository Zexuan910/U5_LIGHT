#include "health_ui.h"

#include <string.h>

static uint8_t gh_status_is_ready(Gh3018HrSpo2Status status)
{
  return ((status == GH3018_HRSPO2_STATUS_WAITING_DATA) ||
          (status == GH3018_HRSPO2_STATUS_RUNNING) ||
          (status == GH3018_HRSPO2_STATUS_RESULT_REFRESHED)) ? 1U : 0U;
}

static uint8_t gh_status_is_error(Gh3018HrSpo2Status status)
{
  return ((status == GH3018_HRSPO2_STATUS_COMM_FAIL) ||
          (status == GH3018_HRSPO2_STATUS_SIMPLE_INIT_FAIL) ||
          (status == GH3018_HRSPO2_STATUS_START_FAIL) ||
          (status == GH3018_HRSPO2_STATUS_CALC_FAIL)) ? 1U : 0U;
}

static uint8_t motion_status_is_error(MotionServiceStatus status)
{
  return ((status == MOTION_SERVICE_STATUS_MPU_INIT_FAIL) ||
          (status == MOTION_SERVICE_STATUS_READ_FAIL)) ? 1U : 0U;
}

void HealthUi_BuildSnapshot(HealthUiSnapshot *snapshot)
{
  const Gh3018HrSpo2Snapshot *hrspo2;
  const MotionServiceSnapshot *motion;

  if (snapshot == 0) {
    return;
  }

  memset(snapshot, 0, sizeof(*snapshot));
  hrspo2 = Gh3018HrSpo2_GetSnapshot();
  motion = MotionService_GetSnapshot();

  snapshot->ghStatus = hrspo2->status;
  snapshot->ghReady = gh_status_is_ready(hrspo2->status);
  snapshot->ghError = gh_status_is_error(hrspo2->status);
  snapshot->heartRateValid = ((hrspo2->resultRefreshCount > 0U) && (hrspo2->heartRate > 0U)) ? 1U : 0U;
  snapshot->spo2Valid = ((hrspo2->resultRefreshCount > 0U) && (hrspo2->spo2 > 0U)) ? 1U : 0U;
  snapshot->heartRate = hrspo2->heartRate;
  snapshot->heartRateConfidence = hrspo2->heartRateConfidence;
  snapshot->spo2 = hrspo2->spo2;
  snapshot->spo2Confidence = hrspo2->spo2Confidence;
  snapshot->wearingState = hrspo2->wearingState;
  snapshot->ghRefreshCount = hrspo2->resultRefreshCount;

  snapshot->motionStatus = motion->status;
  snapshot->mpuStatus = motion->mpuStatus;
  snapshot->mpuReady = motion->mpuReady;
  snapshot->mpuError = motion_status_is_error(motion->status);
  snapshot->steps = motion->steps;
  snapshot->ropeCount = motion->ropeCount;
  snapshot->distanceCm = motion->distanceCm;
  snapshot->instantSpeedCms = motion->instantSpeedCms;
  snapshot->averageSpeedCms = motion->averageSpeedCms;
  snapshot->strideCm = motion->strideCm;
  snapshot->cadenceSpm = motion->cadenceSpm;
  snapshot->activityX100 = motion->activityX100;
  snapshot->motionOutputCount = motion->outputCount;
  snapshot->motionReadFailCount = motion->readFailCount;
  for (uint8_t i = 0U; i < 3U; ++i) {
    snapshot->accelMg[i] = motion->accelMg[i];
    snapshot->gyroMdps[i] = motion->gyroMdps[i];
  }
}

const char *HealthUi_ModeName(HealthUiMode mode)
{
  switch (mode) {
  case HEALTH_UI_MODE_WALK:
    return "WALK";
  case HEALTH_UI_MODE_RUN:
    return "RUN";
  case HEALTH_UI_MODE_ROPE:
    return "ROPE";
  default:
    return "UNKNOWN";
  }
}
