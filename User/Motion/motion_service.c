#include "motion_service.h"

#include "main.h"
#include "walk_metrics.h"

#include <math.h>
#include <string.h>

#define MOTION_POLL_PERIOD_MS 20U

static MotionServiceSnapshot s_snapshot;
static WalkMetricsState s_walkState;
static WalkMetricsOutput s_walkOutput;

static uint16_t float_to_u16_scaled(float value, float scale)
{
  float scaled = value * scale;

  if (scaled <= 0.0f) {
    return 0U;
  }
  if (scaled >= 65535.0f) {
    return 65535U;
  }
  return (uint16_t)(scaled + 0.5f);
}

static uint32_t float_to_u32_scaled(float value, float scale)
{
  float scaled = value * scale;

  if (scaled <= 0.0f) {
    return 0UL;
  }
  if (scaled >= 4294967040.0f) {
    return 4294967040UL;
  }
  return (uint32_t)(scaled + 0.5f);
}

static uint16_t compute_cadence_spm(const WalkMetricsState *state, const WalkMetricsOutput *output)
{
  if ((state->last_step_interval_ms > 0U) && (state->last_step_interval_ms <= 3000U)) {
    return (uint16_t)((60000U + (state->last_step_interval_ms / 2U)) / state->last_step_interval_ms);
  }

  if ((output->elapsed_ms > 0U) && (output->step_count > 0U)) {
    return (uint16_t)(((output->step_count * 60000UL) + (output->elapsed_ms / 2U)) / output->elapsed_ms);
  }

  return 0U;
}

static uint16_t compute_activity_x100(const Mpu6050Snapshot *mpu)
{
  const float accelNorm = sqrtf((mpu->accelG[0] * mpu->accelG[0]) +
                                (mpu->accelG[1] * mpu->accelG[1]) +
                                (mpu->accelG[2] * mpu->accelG[2]));
  const float gyroNorm = sqrtf((mpu->gyroRadS[0] * mpu->gyroRadS[0]) +
                               (mpu->gyroRadS[1] * mpu->gyroRadS[1]) +
                               (mpu->gyroRadS[2] * mpu->gyroRadS[2]));
  float dynamic = accelNorm - 1.0f;
  float activity;

  if (dynamic < 0.0f) {
    dynamic = -dynamic;
  }

  activity = (dynamic * 100.0f) + (gyroNorm * 20.0f);
  if (activity > 655.35f) {
    activity = 655.35f;
  }
  return (uint16_t)(activity * 100.0f + 0.5f);
}

static void copy_mpu_fields(const Mpu6050Snapshot *mpu)
{
  s_snapshot.mpuStatus = mpu->status;
  s_snapshot.mpuReady = (mpu->status == MPU6050_STATUS_READY) ? 1U : 0U;
  s_snapshot.mpuIntLevel = mpu->intLevel;
  s_snapshot.mpuAddress7bit = mpu->address7bit;
  s_snapshot.mpuWhoAmI = mpu->whoAmI;
  s_snapshot.sampleCount = mpu->readCount;
  s_snapshot.readFailCount = mpu->readFailCount;
  s_snapshot.lastSampleTick = mpu->lastReadTick;
  s_snapshot.lastHalStatus = mpu->lastHalStatus;
  s_snapshot.i2cError = mpu->i2cError;

  for (uint8_t i = 0U; i < 3U; ++i) {
    s_snapshot.accelMg[i] = mpu->accelMg[i];
    s_snapshot.gyroMdps[i] = mpu->gyroMdps[i];
    s_snapshot.accelG[i] = mpu->accelG[i];
    s_snapshot.gyroRadS[i] = mpu->gyroRadS[i];
  }
}

static void copy_walk_output(const Mpu6050Snapshot *mpu)
{
  s_snapshot.elapsedMs = s_walkOutput.elapsed_ms;
  s_snapshot.steps = s_walkOutput.step_count;
  s_snapshot.ropeCount = s_walkOutput.step_count;
  s_snapshot.distanceCm = float_to_u32_scaled(s_walkOutput.distance_m, 100.0f);
  s_snapshot.instantSpeedCms = float_to_u16_scaled(s_walkOutput.instant_speed_mps, 100.0f);
  s_snapshot.averageSpeedCms = float_to_u16_scaled(s_walkOutput.average_speed_mps, 100.0f);
  s_snapshot.strideCm = float_to_u16_scaled(s_walkOutput.stride_m, 100.0f);
  s_snapshot.cadenceSpm = compute_cadence_spm(&s_walkState, &s_walkOutput);
  s_snapshot.activityX100 = compute_activity_x100(mpu);
  s_snapshot.outputUpdated = s_walkOutput.output_updated;
  if (s_walkOutput.output_updated != 0U) {
    s_snapshot.outputCount++;
  }
}

const MotionServiceSnapshot *MotionService_Init(void)
{
  const uint32_t now = HAL_GetTick();
  const Mpu6050Snapshot *mpu;

  memset(&s_snapshot, 0, sizeof(s_snapshot));
  memset(&s_walkOutput, 0, sizeof(s_walkOutput));
  s_snapshot.status = MOTION_SERVICE_STATUS_IDLE;
  WalkMetrics_Start(&s_walkState, now);

  mpu = Mpu6050_Init();
  copy_mpu_fields(mpu);

  if (mpu->status == MPU6050_STATUS_READY) {
    s_snapshot.status = MOTION_SERVICE_STATUS_WAITING_SAMPLE;
  } else {
    s_snapshot.status = MOTION_SERVICE_STATUS_MPU_INIT_FAIL;
  }

  s_snapshot.lastPollTick = now;
  return &s_snapshot;
}

const MotionServiceSnapshot *MotionService_Poll(uint32_t nowMs)
{
  const Mpu6050Snapshot *mpu;
  WalkMetricsImuSample sample;

  if ((nowMs - s_snapshot.lastPollTick) < MOTION_POLL_PERIOD_MS) {
    s_snapshot.outputUpdated = 0U;
    return &s_snapshot;
  }

  s_snapshot.lastPollTick = nowMs;
  s_snapshot.pollCount++;
  mpu = Mpu6050_Read();
  copy_mpu_fields(mpu);

  if (mpu->sampleReady == 0U) {
    s_snapshot.outputUpdated = 0U;
    s_snapshot.status = (mpu->status == MPU6050_STATUS_READY) ?
        MOTION_SERVICE_STATUS_WAITING_SAMPLE :
        MOTION_SERVICE_STATUS_READ_FAIL;
    return &s_snapshot;
  }

  sample.tick_ms = nowMs;
  for (uint8_t i = 0U; i < 3U; ++i) {
    sample.accel_g[i] = mpu->accelG[i];
    sample.gyro_rad_s[i] = mpu->gyroRadS[i];
  }

  WalkMetrics_Update(&s_walkState, &sample, &s_walkOutput);
  copy_walk_output(mpu);
  s_snapshot.status = MOTION_SERVICE_STATUS_RUNNING;
  return &s_snapshot;
}

const MotionServiceSnapshot *MotionService_GetSnapshot(void)
{
  return &s_snapshot;
}

const char *MotionService_StatusName(MotionServiceStatus status)
{
  switch (status) {
  case MOTION_SERVICE_STATUS_IDLE:
    return "IDLE";
  case MOTION_SERVICE_STATUS_MPU_INIT_FAIL:
    return "MPU_INIT_FAIL";
  case MOTION_SERVICE_STATUS_WAITING_SAMPLE:
    return "WAITING_SAMPLE";
  case MOTION_SERVICE_STATUS_RUNNING:
    return "RUNNING";
  case MOTION_SERVICE_STATUS_READ_FAIL:
    return "READ_FAIL";
  default:
    return "UNKNOWN";
  }
}
