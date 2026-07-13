#include "walk_metrics.h"

#include "psram.h"
#include "stm32u5xx.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define WALK_OUTPUT_PERIOD_MS          100U
#define WALK_START_SETTLE_MS           400U
#define WALK_MIN_STEP_INTERVAL_MS      300U
#define WALK_MAX_STEP_INTERVAL_MS      1800U
#define WALK_GAIT_RESET_MS             2200U
#define WALK_STEP_EVENT_TIMEOUT_MS     700U
#define WALK_PEAK_FALL_MIN_MS           80U
#define WALK_CONFIRM_STEP_COUNT        3U
#define WALK_SPEED_HOLD_MS             1800U
#define WALK_DEFAULT_STRIDE_M          0.62f
#define WALK_MIN_STRIDE_M              0.42f
#define WALK_MAX_STRIDE_M              0.88f
#define WALK_GRAVITY_TAU_MS            480.0f
#define WALK_SIGNAL_TAU_MS             55.0f
#define WALK_NOISE_TAU_MS              2000.0f
#define WALK_ACCEL_MOTION_FLOOR_G      0.06f
#define WALK_ACCEL_PEAK_FLOOR_G        0.14f
#define WALK_GYRO_GATE_FLOOR_RAD_S     0.10f
#define WALK_GYRO_PEAK_FLOOR_RAD_S     0.42f

volatile uint32_t g_walk_metrics_debug_write_index = 0UL;
volatile uint32_t g_walk_metrics_debug_count = 0UL;
volatile uint32_t g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_NOT_READY;
volatile uint32_t g_walk_metrics_debug_error_count = 0UL;
volatile uint32_t g_walk_metrics_debug_session_count = 0UL;
volatile uint32_t g_walk_metrics_debug_active_session_index =
  WALK_METRICS_DEBUG_NO_ACTIVE_SESSION;
volatile WalkMetricsDebugSession
  g_walk_metrics_debug_sessions[WALK_METRICS_DEBUG_MAX_SESSIONS];
volatile WalkMetricsDebugExportControl g_walk_metrics_debug_export_control = {
  0UL, 0UL, 0UL, 0UL, WALK_METRICS_DEBUG_EXPORT_IDLE
};
uint8_t g_walk_metrics_debug_export_buffer[WALK_METRICS_DEBUG_EXPORT_BUFFER_BYTES];

_Static_assert(sizeof(WalkMetricsRawDebugSample) == 16U,
               "Walk debug record layout must remain 16 bytes");
_Static_assert(sizeof(WalkMetricsDebugSession) == 20U,
               "Walk debug session layout must remain 20 bytes");
_Static_assert((WALK_METRICS_DEBUG_CAPACITY * sizeof(WalkMetricsRawDebugSample)) ==
                 PSRAM_SIZE_BYTES,
               "Walk debug ring must match PSRAM capacity");

static float clampf(float value, float min_value, float max_value)
{
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return value;
}

static float maxf(float a, float b)
{
  return (a > b) ? a : b;
}

static float filter_alpha(float tau_ms, uint32_t dt_ms)
{
  const float dt = (dt_ms == 0U) ? 1.0f : (float)dt_ms;
  return tau_ms / (tau_ms + dt);
}

static float vector_norm3(const float v[3])
{
  return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static float estimate_step_length_m(uint32_t interval_ms, float peak_g)
{
  float cadence_spm;
  float stride = WALK_DEFAULT_STRIDE_M;

  if ((interval_ms > 0U) && (interval_ms <= WALK_MAX_STEP_INTERVAL_MS))
  {
    cadence_spm = 60000.0f / (float)interval_ms;
  }
  else
  {
    cadence_spm = 100.0f;
  }

  stride += (cadence_spm - 100.0f) * 0.0015f;
  stride += (peak_g - WALK_ACCEL_PEAK_FLOOR_G) * 0.35f;
  return clampf(stride, WALK_MIN_STRIDE_M, WALK_MAX_STRIDE_M);
}

static void fill_output(const WalkMetricsState* state, uint32_t tick_ms,
                        WalkMetricsOutput* output, uint8_t updated)
{
  const uint32_t elapsed_ms = (state->running != 0U) ? (tick_ms - state->start_tick_ms) : 0U;
  const float elapsed_s = (elapsed_ms > 0U) ? ((float)elapsed_ms / 1000.0f) : 0.0f;
  const uint8_t moving_recently = ((state->last_motion_tick_ms != 0U) &&
                                   ((tick_ms - state->last_motion_tick_ms) <= WALK_SPEED_HOLD_MS)) ? 1U : 0U;

  output->elapsed_ms = elapsed_ms;
  output->step_count = state->step_count;
  output->distance_m = state->distance_m;
  output->stride_m = (state->step_count > 0U) ? state->average_step_length_m : 0.0f;
  output->average_speed_mps = (elapsed_s > 0.0f) ? (state->distance_m / elapsed_s) : 0.0f;
  output->instant_speed_mps = (moving_recently != 0U) ? state->instant_speed_mps : 0.0f;
  output->output_updated = updated;
}

static void clear_candidate_chain(WalkMetricsState* state)
{
  state->gait_confirmed = 0U;
  state->candidate_count = 0U;
  state->last_candidate_tick_ms = 0U;
  state->last_candidate_interval_ms = 0U;
  memset(state->candidate_stride_m, 0, sizeof(state->candidate_stride_m));
  memset(state->candidate_interval_ms, 0, sizeof(state->candidate_interval_ms));
}

static uint8_t interval_is_consistent(uint32_t interval_ms, uint32_t previous_ms)
{
  uint32_t difference;
  uint32_t tolerance;

  if ((interval_ms < WALK_MIN_STEP_INTERVAL_MS) ||
      (interval_ms > WALK_MAX_STEP_INTERVAL_MS) ||
      (previous_ms == 0U))
  {
    return (previous_ms == 0U) ? 1U : 0U;
  }

  difference = (interval_ms > previous_ms) ? (interval_ms - previous_ms) : (previous_ms - interval_ms);
  tolerance = previous_ms * 55U / 100U;
  if (tolerance < 240U)
  {
    tolerance = 240U;
  }
  return (difference <= tolerance) ? 1U : 0U;
}

static void commit_step(WalkMetricsState* state, uint32_t interval_ms,
                        float step_length_m, uint32_t tick_ms)
{
  const float raw_speed_mps = step_length_m * 1000.0f / (float)interval_ms;

  if (state->step_count == 0U)
  {
    state->average_step_length_m = step_length_m;
    state->instant_speed_mps = raw_speed_mps;
  }
  else
  {
    state->average_step_length_m = state->average_step_length_m * 0.75f + step_length_m * 0.25f;
    state->instant_speed_mps = state->instant_speed_mps * 0.65f + raw_speed_mps * 0.35f;
  }

  state->last_step_length_m = state->average_step_length_m;
  state->distance_m += state->last_step_length_m;
  state->step_count++;
  state->last_step_tick_ms = tick_ms;
  state->last_step_interval_ms = interval_ms;
  state->last_motion_tick_ms = tick_ms;
}

static void start_candidate_chain(WalkMetricsState* state, uint32_t tick_ms,
                                  uint32_t interval_ms, float stride_m)
{
  clear_candidate_chain(state);
  state->candidate_count = 1U;
  state->candidate_interval_ms[0] = interval_ms;
  state->candidate_stride_m[0] = stride_m;
  state->last_candidate_tick_ms = tick_ms;
  state->last_candidate_interval_ms = 0U;
}

static void accept_candidate_step(WalkMetricsState* state, uint32_t tick_ms, float peak_dynamic_g)
{
  uint32_t interval_ms;
  float stride_m;

  if ((state->last_candidate_tick_ms == 0U) ||
      ((tick_ms - state->last_candidate_tick_ms) > WALK_MAX_STEP_INTERVAL_MS))
  {
    start_candidate_chain(state, tick_ms, 600U,
                          estimate_step_length_m(600U, peak_dynamic_g));
    return;
  }

  interval_ms = tick_ms - state->last_candidate_tick_ms;
  stride_m = estimate_step_length_m(interval_ms, peak_dynamic_g);

  if (state->gait_confirmed != 0U)
  {
    if ((interval_ms >= WALK_MIN_STEP_INTERVAL_MS) &&
        (interval_ms <= WALK_MAX_STEP_INTERVAL_MS))
    {
      if ((state->last_candidate_interval_ms != 0U) &&
          (interval_ms > (state->last_candidate_interval_ms * 8U / 5U)) &&
          (interval_ms < (state->last_candidate_interval_ms * 5U / 2U)))
      {
        const uint32_t recovered_interval_ms = interval_ms / 2U;
        const float recovered_stride_m = estimate_step_length_m(recovered_interval_ms, peak_dynamic_g);
        commit_step(state, recovered_interval_ms, recovered_stride_m, tick_ms);
        commit_step(state, recovered_interval_ms, recovered_stride_m, tick_ms);
        state->last_candidate_interval_ms = recovered_interval_ms;
      }
      else
      {
        commit_step(state, interval_ms, stride_m, tick_ms);
        state->last_candidate_interval_ms = interval_ms;
      }
      state->last_candidate_tick_ms = tick_ms;
      return;
    }

    start_candidate_chain(state, tick_ms, 600U,
                          estimate_step_length_m(600U, peak_dynamic_g));
    return;
  }

  if (interval_is_consistent(interval_ms, state->last_candidate_interval_ms) == 0U)
  {
    start_candidate_chain(state, tick_ms, interval_ms, stride_m);
    return;
  }

  state->last_candidate_tick_ms = tick_ms;
  state->last_candidate_interval_ms = interval_ms;

  if (state->candidate_count < WALK_CONFIRM_STEP_COUNT)
  {
    const uint8_t index = state->candidate_count;
    state->candidate_interval_ms[index] = interval_ms;
    state->candidate_stride_m[index] = stride_m;
    state->candidate_count++;
  }

  if (state->candidate_count >= WALK_CONFIRM_STEP_COUNT)
  {
    for (uint8_t index = 0U; index < WALK_CONFIRM_STEP_COUNT; index++)
    {
      commit_step(state,
                  state->candidate_interval_ms[index],
                  state->candidate_stride_m[index],
                  tick_ms);
    }
    state->gait_confirmed = 1U;
  }
}

void WalkMetrics_DebugClear(void)
{
  g_walk_metrics_debug_write_index = 0UL;
  g_walk_metrics_debug_count = 0UL;
  g_walk_metrics_debug_error_count = 0UL;
  g_walk_metrics_debug_session_count = 0UL;
  g_walk_metrics_debug_active_session_index = WALK_METRICS_DEBUG_NO_ACTIVE_SESSION;
}

void WalkMetrics_DebugStorageInit(void)
{
  WalkMetrics_DebugClear();
  g_walk_metrics_debug_storage_status =
    (PSRAM_IsReady() != 0U) ? WALK_METRICS_DEBUG_STORAGE_READY :
                              WALK_METRICS_DEBUG_STORAGE_NOT_READY;
  g_walk_metrics_debug_export_control.psram_address = 0UL;
  g_walk_metrics_debug_export_control.length = 0UL;
  g_walk_metrics_debug_export_control.request_id = 0UL;
  g_walk_metrics_debug_export_control.completed_id = 0UL;
  g_walk_metrics_debug_export_control.status = WALK_METRICS_DEBUG_EXPORT_IDLE;
}

void WalkMetrics_DebugBeginSession(uint32_t tick_ms)
{
  uint32_t session_index;
  volatile WalkMetricsDebugSession* session;

  if (g_walk_metrics_debug_active_session_index != WALK_METRICS_DEBUG_NO_ACTIVE_SESSION)
  {
    WalkMetrics_DebugEndSession(tick_ms);
  }

  if ((PSRAM_IsReady() == 0U) ||
      (g_walk_metrics_debug_write_index >= WALK_METRICS_DEBUG_CAPACITY) ||
      (g_walk_metrics_debug_session_count >= WALK_METRICS_DEBUG_MAX_SESSIONS))
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
    g_walk_metrics_debug_error_count++;
    return;
  }

  session_index = g_walk_metrics_debug_session_count;
  session = &g_walk_metrics_debug_sessions[session_index];
  session->start_index = g_walk_metrics_debug_write_index;
  session->sample_count = 0UL;
  session->start_tick_ms = tick_ms;
  session->end_tick_ms = tick_ms;
  session->status = WALK_METRICS_DEBUG_SESSION_ACTIVE;
  g_walk_metrics_debug_active_session_index = session_index;
  g_walk_metrics_debug_session_count = session_index + 1UL;
}

void WalkMetrics_DebugEndSession(uint32_t tick_ms)
{
  uint32_t session_index = g_walk_metrics_debug_active_session_index;

  if ((session_index == WALK_METRICS_DEBUG_NO_ACTIVE_SESSION) ||
      (session_index >= g_walk_metrics_debug_session_count))
  {
    return;
  }

  g_walk_metrics_debug_sessions[session_index].end_tick_ms = tick_ms;
  g_walk_metrics_debug_sessions[session_index].status =
    WALK_METRICS_DEBUG_SESSION_COMPLETE;
  g_walk_metrics_debug_active_session_index = WALK_METRICS_DEBUG_NO_ACTIVE_SESSION;
}

void WalkMetrics_DebugServiceExport(void)
{
  uint32_t request_id = g_walk_metrics_debug_export_control.request_id;
  uint32_t address;
  uint32_t length;
  uint8_t success = 0U;

  if ((request_id == 0UL) ||
      (request_id == g_walk_metrics_debug_export_control.completed_id))
  {
    return;
  }

  address = g_walk_metrics_debug_export_control.psram_address;
  length = g_walk_metrics_debug_export_control.length;
  g_walk_metrics_debug_export_control.status = WALK_METRICS_DEBUG_EXPORT_BUSY;

  if ((PSRAM_IsReady() != 0U) &&
      (length > 0UL) &&
      (length <= WALK_METRICS_DEBUG_EXPORT_BUFFER_BYTES) &&
      (address < PSRAM_SIZE_BYTES) &&
      ((PSRAM_SIZE_BYTES - address) >= length))
  {
    success = PSRAM_Read(address, g_walk_metrics_debug_export_buffer, length);
  }

  g_walk_metrics_debug_export_control.status =
    (success != 0U) ? WALK_METRICS_DEBUG_EXPORT_READY :
                      WALK_METRICS_DEBUG_EXPORT_ERROR;
  __DMB();
  g_walk_metrics_debug_export_control.completed_id = request_id;
}

void WalkMetrics_DebugRecordRaw(uint32_t tick_ms, const int16_t accel_raw[3],
                                const int16_t gyro_raw[3])
{
  WalkMetricsRawDebugSample sample;
  uint32_t index;

  if ((accel_raw == NULL) || (gyro_raw == NULL))
  {
    return;
  }

  if (PSRAM_IsReady() == 0U)
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_NOT_READY;
    g_walk_metrics_debug_error_count++;
    return;
  }

  if (g_walk_metrics_debug_active_session_index == WALK_METRICS_DEBUG_NO_ACTIVE_SESSION)
  {
    WalkMetrics_DebugBeginSession(tick_ms);
  }
  if (g_walk_metrics_debug_active_session_index == WALK_METRICS_DEBUG_NO_ACTIVE_SESSION)
  {
    return;
  }

  index = g_walk_metrics_debug_write_index;
  if (index >= WALK_METRICS_DEBUG_CAPACITY)
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
    g_walk_metrics_debug_error_count++;
    return;
  }
  sample.tick_ms = tick_ms;
  for (uint8_t axis = 0U; axis < 3U; axis++)
  {
    sample.accel_raw[axis] = accel_raw[axis];
    sample.gyro_raw[axis] = gyro_raw[axis];
  }

  if (PSRAM_Write(index * sizeof(sample), (const uint8_t*)&sample,
                  sizeof(sample)) == 0U)
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
    g_walk_metrics_debug_error_count++;
    return;
  }

  index++;
  g_walk_metrics_debug_write_index = index;
  g_walk_metrics_debug_count = index;
  g_walk_metrics_debug_sessions[g_walk_metrics_debug_active_session_index].sample_count++;
  g_walk_metrics_debug_sessions[g_walk_metrics_debug_active_session_index].end_tick_ms = tick_ms;
  g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_READY;
}

void WalkMetrics_Reset(WalkMetricsState* state)
{
  if (state == NULL)
  {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->gravity_g = 1.0f;
  state->noise_dynamic_g = 0.025f;
  state->noise_gyro_rad_s = 0.04f;
  state->last_step_length_m = WALK_DEFAULT_STRIDE_M;
  state->average_step_length_m = WALK_DEFAULT_STRIDE_M;
}

void WalkMetrics_Start(WalkMetricsState* state, uint32_t tick_ms)
{
  WalkMetrics_Reset(state);
  if (state == NULL)
  {
    return;
  }

  state->running = 1U;
  state->start_tick_ms = tick_ms;
  state->last_sample_tick_ms = tick_ms;
  state->last_output_tick_ms = tick_ms;
  WalkMetrics_DebugBeginSession(tick_ms);
}

void WalkMetrics_Stop(WalkMetricsState* state)
{
  if (state == NULL)
  {
    return;
  }

  state->running = 0U;
  state->step_active = 0U;
  WalkMetrics_DebugEndSession(state->last_sample_tick_ms);
}

void WalkMetrics_SuppressMotion(WalkMetricsState* state, uint32_t tick_ms,
                                WalkMetricsOutput* output)
{
  uint8_t output_updated = 0U;

  if ((state == NULL) || (output == NULL))
  {
    return;
  }

  clear_candidate_chain(state);
  state->step_active = 0U;
  state->step_start_tick_ms = 0U;
  state->peak_tick_ms = 0U;
  state->peak_dynamic_g = 0.0f;
  state->peak_gyro_norm_rad_s = 0.0f;
  state->instant_speed_mps = 0.0f;
  state->last_motion_tick_ms = 0U;
  state->last_sample_tick_ms = tick_ms;

  if ((tick_ms - state->last_output_tick_ms) >= WALK_OUTPUT_PERIOD_MS)
  {
    state->last_output_tick_ms = tick_ms;
    output_updated = 1U;
  }
  fill_output(state, tick_ms, output, output_updated);
}

void WalkMetrics_Update(WalkMetricsState* state, const WalkMetricsImuSample* sample,
                        WalkMetricsOutput* output)
{
  float accel_norm_g;
  float gyro_norm_rad_s;
  float dynamic_g;
  float abs_dynamic_g;
  float gravity_alpha;
  float signal_alpha;
  float accel_motion_threshold;
  float accel_peak_threshold;
  float gyro_gate_threshold;
  float gyro_peak_threshold;
  uint32_t dt_ms;
  uint8_t strong_motion_event;
  uint8_t output_updated = 0U;

  if ((state == NULL) || (sample == NULL) || (output == NULL))
  {
    return;
  }

  if (state->running == 0U)
  {
    fill_output(state, sample->tick_ms, output, 0U);
    return;
  }

  dt_ms = sample->tick_ms - state->last_sample_tick_ms;
  if ((dt_ms == 0U) || (dt_ms > 200U))
  {
    dt_ms = 20U;
  }

  accel_norm_g = vector_norm3(sample->accel_g);
  gyro_norm_rad_s = vector_norm3(sample->gyro_rad_s);
  gravity_alpha = filter_alpha(WALK_GRAVITY_TAU_MS, dt_ms);
  signal_alpha = filter_alpha(WALK_SIGNAL_TAU_MS, dt_ms);
  state->gravity_g = gravity_alpha * state->gravity_g + (1.0f - gravity_alpha) * accel_norm_g;
  dynamic_g = accel_norm_g - state->gravity_g;
  state->filtered_dynamic_g = signal_alpha * state->filtered_dynamic_g +
                              (1.0f - signal_alpha) * dynamic_g;
  state->filtered_gyro_rad_s = signal_alpha * state->filtered_gyro_rad_s +
                               (1.0f - signal_alpha) * gyro_norm_rad_s;
  abs_dynamic_g = fabsf(state->filtered_dynamic_g);

  if ((state->step_active == 0U) &&
      (abs_dynamic_g < 0.08f) &&
      (state->filtered_gyro_rad_s < 0.18f))
  {
    const float noise_alpha = filter_alpha(WALK_NOISE_TAU_MS, dt_ms);
    state->noise_dynamic_g = noise_alpha * state->noise_dynamic_g +
                             (1.0f - noise_alpha) * abs_dynamic_g;
    state->noise_gyro_rad_s = noise_alpha * state->noise_gyro_rad_s +
                              (1.0f - noise_alpha) * state->filtered_gyro_rad_s;
    state->noise_dynamic_g = clampf(state->noise_dynamic_g, 0.015f, 0.05f);
    state->noise_gyro_rad_s = clampf(state->noise_gyro_rad_s, 0.025f, 0.10f);
  }

  accel_motion_threshold = maxf(WALK_ACCEL_MOTION_FLOOR_G, state->noise_dynamic_g * 3.0f);
  accel_peak_threshold = maxf(WALK_ACCEL_PEAK_FLOOR_G, state->noise_dynamic_g * 5.0f);
  gyro_gate_threshold = maxf(WALK_GYRO_GATE_FLOOR_RAD_S, state->noise_gyro_rad_s * 3.0f);
  gyro_peak_threshold = maxf(WALK_GYRO_PEAK_FLOOR_RAD_S, state->noise_gyro_rad_s * 5.0f);

  if ((sample->tick_ms - state->start_tick_ms) < WALK_START_SETTLE_MS)
  {
    state->prev_dynamic_g = state->filtered_dynamic_g;
    state->prev_gyro_norm_rad_s = state->filtered_gyro_rad_s;
    state->last_sample_tick_ms = sample->tick_ms;
    fill_output(state, sample->tick_ms, output, 0U);
    return;
  }

  if ((abs_dynamic_g >= accel_motion_threshold) ||
      (state->filtered_gyro_rad_s >= gyro_gate_threshold))
  {
    state->last_motion_tick_ms = sample->tick_ms;
  }

  if ((state->last_candidate_tick_ms != 0U) &&
      ((sample->tick_ms - state->last_candidate_tick_ms) > WALK_GAIT_RESET_MS))
  {
    clear_candidate_chain(state);
  }

  strong_motion_event = (((abs_dynamic_g >= accel_peak_threshold) &&
                          (state->filtered_gyro_rad_s >= gyro_gate_threshold)) ||
                         ((state->filtered_gyro_rad_s >= gyro_peak_threshold) &&
                          (abs_dynamic_g >= accel_motion_threshold))) ? 1U : 0U;

  if ((state->step_active == 0U) && (strong_motion_event != 0U) &&
      ((state->last_candidate_tick_ms == 0U) ||
       ((sample->tick_ms - state->last_candidate_tick_ms) >= WALK_MIN_STEP_INTERVAL_MS)))
  {
    state->step_active = 1U;
    state->step_start_tick_ms = sample->tick_ms;
    state->peak_tick_ms = sample->tick_ms;
    state->peak_dynamic_g = abs_dynamic_g;
    state->peak_gyro_norm_rad_s = state->filtered_gyro_rad_s;
  }
  else if (state->step_active != 0U)
  {
    if (abs_dynamic_g > state->peak_dynamic_g)
    {
      state->peak_dynamic_g = abs_dynamic_g;
      state->peak_tick_ms = sample->tick_ms;
    }
    if (state->filtered_gyro_rad_s > state->peak_gyro_norm_rad_s)
    {
      state->peak_gyro_norm_rad_s = state->filtered_gyro_rad_s;
    }

    if ((((sample->tick_ms - state->step_start_tick_ms) >= WALK_PEAK_FALL_MIN_MS) &&
         (((state->peak_gyro_norm_rad_s >= gyro_peak_threshold) &&
           (state->filtered_gyro_rad_s <= (state->peak_gyro_norm_rad_s * 0.72f))) ||
          ((state->peak_dynamic_g >= accel_peak_threshold) &&
           (abs_dynamic_g <= (state->peak_dynamic_g * 0.62f))))) ||
        ((abs_dynamic_g <= maxf(0.10f, accel_motion_threshold)) &&
         (state->filtered_gyro_rad_s <= maxf(0.30f, gyro_gate_threshold * 1.5f))))
    {
      accept_candidate_step(state, state->peak_tick_ms, state->peak_dynamic_g);
      state->step_active = 0U;
      state->peak_dynamic_g = 0.0f;
      state->peak_gyro_norm_rad_s = 0.0f;
    }
    else if ((sample->tick_ms - state->step_start_tick_ms) > WALK_STEP_EVENT_TIMEOUT_MS)
    {
      accept_candidate_step(state, state->peak_tick_ms, state->peak_dynamic_g);
      state->step_active = 0U;
      state->peak_dynamic_g = 0.0f;
      state->peak_gyro_norm_rad_s = 0.0f;
    }
  }

  if ((sample->tick_ms - state->last_output_tick_ms) >= WALK_OUTPUT_PERIOD_MS)
  {
    state->last_output_tick_ms = sample->tick_ms;
    output_updated = 1U;
  }

  state->prev_dynamic_g = state->filtered_dynamic_g;
  state->prev_gyro_norm_rad_s = state->filtered_gyro_rad_s;
  state->last_sample_tick_ms = sample->tick_ms;
  fill_output(state, sample->tick_ms, output, output_updated);
}
