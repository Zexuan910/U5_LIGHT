#include "walk_metrics.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define WALK_OUTPUT_PERIOD_MS      500U
#define WALK_START_SETTLE_MS       300U
#define WALK_MIN_STEP_INTERVAL_MS  320U
#define WALK_MAX_STEP_INTERVAL_MS  1800U
#define WALK_PEAK_THRESHOLD_G      0.26f
#define WALK_MOTION_THRESHOLD_G    0.12f
#define WALK_GYRO_GATE_RAD_S       0.18f
#define WALK_GYRO_PEAK_RAD_S       0.70f
#define WALK_REARM_THRESHOLD_G     0.10f
#define WALK_GYRO_REARM_RAD_S      0.24f
#define WALK_SPEED_HOLD_MS         1800U
#define WALK_DEFAULT_STRIDE_M      0.62f
#define WALK_MIN_STRIDE_M          0.42f
#define WALK_MAX_STRIDE_M          0.88f
#define WALK_GRAVITY_ALPHA         0.96f

static float clampf_local(float value, float min_value, float max_value)
{
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static float vector_norm3(const float v[3])
{
  return sqrtf((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
}

static float estimate_step_length_m(uint32_t interval_ms, float peak_g)
{
  float cadence_spm;
  float stride = WALK_DEFAULT_STRIDE_M;

  if ((interval_ms > 0U) && (interval_ms <= WALK_MAX_STEP_INTERVAL_MS)) {
    cadence_spm = 60000.0f / (float)interval_ms;
  } else {
    cadence_spm = 100.0f;
  }

  stride += (cadence_spm - 100.0f) * 0.0015f;
  stride += (peak_g - WALK_PEAK_THRESHOLD_G) * 0.35f;
  return clampf_local(stride, WALK_MIN_STRIDE_M, WALK_MAX_STRIDE_M);
}

static void fill_output(const WalkMetricsState *state, uint32_t tick_ms, WalkMetricsOutput *output, uint8_t updated)
{
  const uint32_t elapsed_ms = (state->running != 0U) ? (tick_ms - state->start_tick_ms) : 0U;
  const float elapsed_s = (elapsed_ms > 0U) ? ((float)elapsed_ms / 1000.0f) : 0.0f;
  const uint8_t moving_recently =
      ((state->last_motion_tick_ms != 0U) && ((tick_ms - state->last_motion_tick_ms) <= WALK_SPEED_HOLD_MS)) ? 1U : 0U;

  output->elapsed_ms = elapsed_ms;
  output->step_count = state->step_count;
  output->distance_m = state->distance_m;
  output->stride_m = (state->step_count > 0U) ? state->average_step_length_m : 0.0f;
  output->average_speed_mps = (elapsed_s > 0.0f) ? (state->distance_m / elapsed_s) : 0.0f;
  output->instant_speed_mps = (moving_recently != 0U) ? state->instant_speed_mps : 0.0f;
  output->output_updated = updated;
}

void WalkMetrics_Reset(WalkMetricsState *state)
{
  if (state == NULL) {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->gravity_g = 1.0f;
  state->last_step_length_m = WALK_DEFAULT_STRIDE_M;
  state->average_step_length_m = WALK_DEFAULT_STRIDE_M;
}

void WalkMetrics_Start(WalkMetricsState *state, uint32_t tick_ms)
{
  WalkMetrics_Reset(state);
  if (state == NULL) {
    return;
  }

  state->running = 1U;
  state->start_tick_ms = tick_ms;
  state->last_sample_tick_ms = tick_ms;
  state->last_output_tick_ms = tick_ms;
  state->window_start_tick_ms = tick_ms;
  state->last_motion_tick_ms = 0U;
}

void WalkMetrics_Stop(WalkMetricsState *state)
{
  if (state == NULL) {
    return;
  }

  state->running = 0U;
}

void WalkMetrics_Update(WalkMetricsState *state, const WalkMetricsImuSample *sample, WalkMetricsOutput *output)
{
  float accel_norm_g;
  float gyro_norm_rad_s;
  float dynamic_g;
  float abs_dynamic_g;
  uint32_t since_last_step_ms;
  uint8_t strong_motion_event;
  uint8_t can_count_step;
  uint8_t output_updated = 0U;

  if ((state == NULL) || (sample == NULL) || (output == NULL)) {
    return;
  }

  if (state->running == 0U) {
    fill_output(state, sample->tick_ms, output, 0U);
    return;
  }

  accel_norm_g = vector_norm3(sample->accel_g);
  gyro_norm_rad_s = vector_norm3(sample->gyro_rad_s);

  state->gravity_g = (WALK_GRAVITY_ALPHA * state->gravity_g) + ((1.0f - WALK_GRAVITY_ALPHA) * accel_norm_g);
  dynamic_g = accel_norm_g - state->gravity_g;
  abs_dynamic_g = fabsf(dynamic_g);

  if ((sample->tick_ms - state->start_tick_ms) < WALK_START_SETTLE_MS) {
    state->prev_dynamic_g = dynamic_g;
    state->prev_gyro_norm_rad_s = gyro_norm_rad_s;
    state->peak_dynamic_g = 0.0f;
    state->peak_gyro_norm_rad_s = 0.0f;
    state->last_sample_tick_ms = sample->tick_ms;
    fill_output(state, sample->tick_ms, output, 0U);
    return;
  }

  if (abs_dynamic_g > state->peak_dynamic_g) {
    state->peak_dynamic_g = abs_dynamic_g;
  }
  if (gyro_norm_rad_s > state->peak_gyro_norm_rad_s) {
    state->peak_gyro_norm_rad_s = gyro_norm_rad_s;
  }

  if ((abs_dynamic_g >= WALK_MOTION_THRESHOLD_G) || (gyro_norm_rad_s >= WALK_GYRO_GATE_RAD_S)) {
    state->last_motion_tick_ms = sample->tick_ms;
  }

  since_last_step_ms = sample->tick_ms - state->last_step_tick_ms;
  strong_motion_event = ((abs_dynamic_g >= WALK_PEAK_THRESHOLD_G) || (gyro_norm_rad_s >= WALK_GYRO_PEAK_RAD_S)) ? 1U : 0U;
  can_count_step = ((state->last_step_tick_ms == 0U) || (since_last_step_ms >= WALK_MIN_STEP_INTERVAL_MS)) ? 1U : 0U;

  if ((can_count_step != 0U) && (state->step_active == 0U) && (strong_motion_event != 0U)) {
    const uint32_t interval_ms = (state->last_step_tick_ms == 0U) ? 600U : since_last_step_ms;
    const float raw_step_length_m = estimate_step_length_m(interval_ms, state->peak_dynamic_g);

    state->last_step_interval_ms = interval_ms;
    if (state->step_count == 0U) {
      state->average_step_length_m = raw_step_length_m;
      state->instant_speed_mps = raw_step_length_m * 1000.0f / (float)interval_ms;
    } else {
      const float raw_speed_mps = raw_step_length_m * 1000.0f / (float)interval_ms;
      state->average_step_length_m = (state->average_step_length_m * 0.75f) + (raw_step_length_m * 0.25f);
      state->instant_speed_mps = (state->instant_speed_mps * 0.65f) + (raw_speed_mps * 0.35f);
    }

    state->last_step_length_m = state->average_step_length_m;
    state->distance_m += state->last_step_length_m;
    state->window_distance_m += state->last_step_length_m;
    state->step_count++;
    state->last_step_tick_ms = sample->tick_ms;
    state->last_motion_tick_ms = sample->tick_ms;
    state->peak_dynamic_g = 0.0f;
    state->peak_gyro_norm_rad_s = 0.0f;
    state->step_active = 1U;
  }

  if ((state->step_active != 0U) &&
      (abs_dynamic_g <= WALK_REARM_THRESHOLD_G) &&
      (gyro_norm_rad_s <= WALK_GYRO_REARM_RAD_S)) {
    state->step_active = 0U;
  }

  if ((sample->tick_ms - state->window_start_tick_ms) >= 3000U) {
    state->window_start_tick_ms = sample->tick_ms;
    state->window_distance_m = 0.0f;
  }

  if ((sample->tick_ms - state->last_output_tick_ms) >= WALK_OUTPUT_PERIOD_MS) {
    state->last_output_tick_ms = sample->tick_ms;
    output_updated = 1U;
  }

  state->prev_dynamic_g = dynamic_g;
  state->prev_gyro_norm_rad_s = gyro_norm_rad_s;
  state->last_sample_tick_ms = sample->tick_ms;
  fill_output(state, sample->tick_ms, output, output_updated);
}
