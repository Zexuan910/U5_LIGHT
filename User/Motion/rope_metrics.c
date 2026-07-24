#include "rope_metrics.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define ROPE_MIN_INTERVAL_MS          200UL
#define ROPE_MAX_INTERVAL_MS         1500UL
#define ROPE_RATE_TIMEOUT_MS         1500UL
#define ROPE_EVENT_MAX_MS             500UL
#define ROPE_HIGH_GYRO_Z_RAD_S         3.50f
#define ROPE_LOW_GYRO_Z_RAD_S          1.40f
#define ROPE_MIN_DYNAMIC_G             0.25f

static float RopeMetrics_Abs(float value)
{
  return (value < 0.0f) ? -value : value;
}

static uint16_t RopeMetrics_ToU16(float value)
{
  if (value <= 0.0f)
  {
    return 0U;
  }
  if (value >= 65535.0f)
  {
    return UINT16_MAX;
  }
  return (uint16_t)(value + 0.5f);
}

static void RopeMetrics_Publish(RopeMetricsState* state,
                                uint32_t tick_ms,
                                RopeMetricsOutput* output)
{
  uint32_t elapsed_ms = tick_ms - state->start_tick_ms;
  float average_rate_x10 = 0.0f;

  output->elapsed_ms = elapsed_ms;
  output->jump_count = state->jump_count;
  if ((state->last_count_tick_ms == 0U) ||
      ((tick_ms - state->last_count_tick_ms) > ROPE_RATE_TIMEOUT_MS))
  {
    output->current_rate_x10 = 0U;
  }
  else
  {
    output->current_rate_x10 =
      RopeMetrics_ToU16(state->smoothed_rate_x10);
  }
  if ((elapsed_ms != 0U) && (state->jump_count != 0U))
  {
    average_rate_x10 =
      ((float)state->jump_count * 600000.0f) / (float)elapsed_ms;
  }
  output->average_rate_x10 = RopeMetrics_ToU16(average_rate_x10);
  output->output_updated = 1U;
}

static void RopeMetrics_AcceptRotation(RopeMetricsState* state,
                                       uint32_t tick_ms)
{
  uint32_t interval_ms = 0U;

  if (state->last_count_tick_ms != 0U)
  {
    interval_ms = tick_ms - state->last_count_tick_ms;
    if (interval_ms < ROPE_MIN_INTERVAL_MS)
    {
      return;
    }
  }

  state->jump_count++;
  if ((interval_ms >= ROPE_MIN_INTERVAL_MS) &&
      (interval_ms <= ROPE_MAX_INTERVAL_MS))
  {
    float rate_x10 = 600000.0f / (float)interval_ms;

    if (state->smoothed_rate_x10 <= 0.0f)
    {
      state->smoothed_rate_x10 = rate_x10;
    }
    else
    {
      state->smoothed_rate_x10 =
        (state->smoothed_rate_x10 * 0.72f) + (rate_x10 * 0.28f);
    }
    state->last_interval_ms = interval_ms;
  }
  else
  {
    state->smoothed_rate_x10 = 0.0f;
    state->last_interval_ms = 0U;
  }
  state->last_count_tick_ms = tick_ms;
}

void RopeMetrics_Reset(RopeMetricsState* state)
{
  if (state == NULL)
  {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->gravity_g = 1.0f;
  state->noise_dynamic_g = 0.02f;
  state->noise_gyro_z_rad_s = 0.04f;
}

void RopeMetrics_Start(RopeMetricsState* state, uint32_t tick_ms)
{
  RopeMetrics_Reset(state);
  if (state == NULL)
  {
    return;
  }

  state->running = 1U;
  state->start_tick_ms = tick_ms;
  state->last_sample_tick_ms = tick_ms;
}

void RopeMetrics_Stop(RopeMetricsState* state)
{
  if (state == NULL)
  {
    return;
  }

  state->running = 0U;
  state->event_active = 0U;
}

void RopeMetrics_Update(RopeMetricsState* state,
                        const RopeMetricsImuSample* sample,
                        RopeMetricsOutput* output)
{
  float accel_norm;
  float dynamic_g;
  float high_gyro_threshold;
  float dynamic_threshold;
  uint32_t event_duration_ms;

  if ((state == NULL) || (sample == NULL) || (output == NULL) ||
      (state->running == 0U))
  {
    return;
  }

  output->output_updated = 0U;
  state->last_sample_tick_ms = sample->tick_ms;
  accel_norm = sqrtf((sample->accel_g[0] * sample->accel_g[0]) +
                     (sample->accel_g[1] * sample->accel_g[1]) +
                     (sample->accel_g[2] * sample->accel_g[2]));
  state->gravity_g += (accel_norm - state->gravity_g) * 0.04f;
  dynamic_g = RopeMetrics_Abs(accel_norm - state->gravity_g);
  state->filtered_dynamic_g +=
    (dynamic_g - state->filtered_dynamic_g) * 0.32f;

  /*
   * The six labelled watch sessions show one stable positive lobe per rope
   * rotation on the watch Z gyro. Counting that signed lobe avoids the
   * previous magnitude detector merging adjacent rotations into one event.
   */
  state->filtered_gyro_z_rad_s +=
    (sample->gyro_rad_s[2] - state->filtered_gyro_z_rad_s) * 0.55f;

  high_gyro_threshold = ROPE_HIGH_GYRO_Z_RAD_S;
  dynamic_threshold = ROPE_MIN_DYNAMIC_G;

  if (state->event_active == 0U)
  {
    if (state->filtered_gyro_z_rad_s >= high_gyro_threshold)
    {
      state->event_active = 1U;
      state->event_start_tick_ms = sample->tick_ms;
      state->event_peak_rotation_rad_s =
        state->filtered_gyro_z_rad_s;
      state->event_peak_dynamic_g = state->filtered_dynamic_g;
    }
  }
  else
  {
    if (state->filtered_gyro_z_rad_s >
        state->event_peak_rotation_rad_s)
    {
      state->event_peak_rotation_rad_s =
        state->filtered_gyro_z_rad_s;
    }
    if (state->filtered_dynamic_g > state->event_peak_dynamic_g)
    {
      state->event_peak_dynamic_g = state->filtered_dynamic_g;
    }

    event_duration_ms = sample->tick_ms - state->event_start_tick_ms;
    if ((state->filtered_gyro_z_rad_s <= ROPE_LOW_GYRO_Z_RAD_S) ||
        (event_duration_ms >= ROPE_EVENT_MAX_MS))
    {
      output->last_peak_gyro_x10 =
        RopeMetrics_ToU16(state->event_peak_rotation_rad_s * 10.0f);
      if ((state->event_peak_rotation_rad_s >=
           high_gyro_threshold) &&
          (state->event_peak_dynamic_g >= dynamic_threshold))
      {
        RopeMetrics_AcceptRotation(state, sample->tick_ms);
      }
      state->event_active = 0U;
      state->event_peak_rotation_rad_s = 0.0f;
      state->event_peak_dynamic_g = 0.0f;
    }
  }

  RopeMetrics_Publish(state, sample->tick_ms, output);
}
