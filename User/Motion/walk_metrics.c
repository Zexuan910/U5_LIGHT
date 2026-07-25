#include "walk_metrics.h"

#include "ext_flash.h"
#include "stm32u5xx.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define WALK_OUTPUT_PERIOD_MS          100U
#define WALK_START_SETTLE_MS           400U
#define WALK_MIN_STEP_INTERVAL_MS      470U
#define WALK_MAX_STEP_INTERVAL_MS      1800U
#define WALK_GAIT_RESET_MS             2200U
#define WALK_STEP_EVENT_TIMEOUT_MS     700U
#define WALK_PEAK_FALL_MIN_MS           80U
#define WALK_CONFIRM_STEP_COUNT        3U
#define WALK_SPEED_HOLD_MS             1800U
#define WALK_DEFAULT_STRIDE_M          0.68f
#define WALK_MIN_STRIDE_M              0.52f
#define WALK_MAX_STRIDE_M              0.86f
#define WALK_GRAVITY_TAU_MS            480.0f
#define WALK_SIGNAL_TAU_MS             55.0f
#define WALK_NOISE_TAU_MS              2000.0f
#define WALK_ACCEL_MOTION_FLOOR_G      0.06f
#define WALK_ACCEL_PEAK_FLOOR_G        0.14f
#define WALK_GYRO_GATE_FLOOR_RAD_S     0.10f
#define WALK_GYRO_PEAK_FLOOR_RAD_S     0.42f

#define FAST_WALK_MIN_STEP_INTERVAL_MS 370U
#define FAST_WALK_DEFAULT_INTERVAL_MS  480U
#define FAST_WALK_DEFAULT_STRIDE_M     0.73f
#define FAST_WALK_MIN_STRIDE_M         0.58f
#define FAST_WALK_MAX_STRIDE_M         0.88f

/* Wrist RUN recordings contain a secondary arm-swing peak near 180-220 ms.
 * Keep the refractory period above that peak while retaining cadences up to
 * about 214 steps/min. The stride constant is calibrated only from the five
 * RUN training sessions; independent test sessions must not tune it. */
#define RUN_MIN_STEP_INTERVAL_MS       280U
#define RUN_MAX_STEP_INTERVAL_MS       1000U
#define RUN_GAIT_RESET_MS              1500U
#define RUN_STEP_EVENT_TIMEOUT_MS       500U
#define RUN_PEAK_FALL_MIN_MS             60U
#define RUN_DEFAULT_INTERVAL_MS         360U
#define RUN_DEFAULT_STRIDE_M           1.04f
#define RUN_MIN_STRIDE_M               0.80f
#define RUN_MAX_STRIDE_M               1.30f
#define RUN_ACCEL_MOTION_FLOOR_G       0.08f
#define RUN_ACCEL_PEAK_FLOOR_G         0.18f
#define RUN_GYRO_GATE_FLOOR_RAD_S      0.12f
#define RUN_GYRO_PEAK_FLOOR_RAD_S      0.50f

#define WALK_FLASH_META_MAGIC          0x574D4631UL
#define WALK_FLASH_META_VERSION        1UL
#define WALK_FLASH_RECORDS_PER_SECTOR  (EXT_FLASH_SECTOR_SIZE_BYTES / sizeof(WalkMetricsRawDebugSample))

volatile uint32_t g_walk_metrics_debug_write_index = 0UL;
volatile uint32_t g_walk_metrics_debug_count = 0UL;
volatile uint32_t g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_NOT_READY;
volatile uint32_t g_walk_metrics_debug_error_count = 0UL;
volatile uint32_t g_walk_metrics_debug_session_count = 0UL;
volatile uint32_t g_walk_metrics_debug_active_session_index =
  WALK_METRICS_DEBUG_NO_ACTIVE_SESSION;
volatile uint32_t g_walk_metrics_flash_generation = 0UL;
volatile uint32_t g_walk_metrics_flash_recovery_count = 0UL;
volatile WalkMetricsDebugSession
  g_walk_metrics_debug_sessions[WALK_METRICS_DEBUG_MAX_SESSIONS];
volatile WalkMetricsDebugExportControl g_walk_metrics_debug_export_control = {
  0UL, 0UL, 0UL, 0UL, WALK_METRICS_DEBUG_EXPORT_IDLE
};
uint8_t g_walk_metrics_debug_export_buffer[WALK_METRICS_DEBUG_EXPORT_BUFFER_BYTES];

typedef __PACKED_STRUCT
{
  uint32_t magic;
  uint32_t version;
  uint32_t generation;
  uint32_t write_index;
  uint32_t sample_count;
  uint32_t session_count;
  uint32_t active_session_index;
  WalkMetricsDebugSession sessions[WALK_METRICS_DEBUG_MAX_SESSIONS];
  uint32_t crc32;
} WalkMetricsFlashMetadata;

static WalkMetricsFlashMetadata walk_flash_metadata;
static WalkMetricsFlashMetadata walk_flash_verify;

typedef struct
{
  uint32_t min_step_interval_ms;
  uint32_t max_step_interval_ms;
  uint32_t gait_reset_ms;
  uint32_t step_event_timeout_ms;
  uint32_t peak_fall_min_ms;
  uint32_t default_interval_ms;
  float default_stride_m;
  float min_stride_m;
  float max_stride_m;
  float accel_motion_floor_g;
  float accel_peak_floor_g;
  float gyro_gate_floor_rad_s;
  float gyro_peak_floor_rad_s;
  float cadence_reference_spm;
  float cadence_stride_gain;
  float peak_stride_gain;
  uint32_t interval_tolerance_percent;
  uint32_t interval_tolerance_floor_ms;
  uint8_t recover_missed_step;
} WalkMetricsProfile;

static const WalkMetricsProfile walk_profile = {
  WALK_MIN_STEP_INTERVAL_MS, WALK_MAX_STEP_INTERVAL_MS,
  WALK_GAIT_RESET_MS, WALK_STEP_EVENT_TIMEOUT_MS, WALK_PEAK_FALL_MIN_MS,
  600U, WALK_DEFAULT_STRIDE_M, WALK_MIN_STRIDE_M, WALK_MAX_STRIDE_M,
  WALK_ACCEL_MOTION_FLOOR_G, WALK_ACCEL_PEAK_FLOOR_G,
  WALK_GYRO_GATE_FLOOR_RAD_S, WALK_GYRO_PEAK_FLOOR_RAD_S,
  100.0f, 0.0f, 0.0f,
  30U, 120U, 0U
};

static const WalkMetricsProfile fast_walk_profile = {
  FAST_WALK_MIN_STEP_INTERVAL_MS, WALK_MAX_STEP_INTERVAL_MS,
  WALK_GAIT_RESET_MS, WALK_STEP_EVENT_TIMEOUT_MS, WALK_PEAK_FALL_MIN_MS,
  FAST_WALK_DEFAULT_INTERVAL_MS, FAST_WALK_DEFAULT_STRIDE_M,
  FAST_WALK_MIN_STRIDE_M, FAST_WALK_MAX_STRIDE_M,
  WALK_ACCEL_MOTION_FLOOR_G, WALK_ACCEL_PEAK_FLOOR_G,
  WALK_GYRO_GATE_FLOOR_RAD_S, WALK_GYRO_PEAK_FLOOR_RAD_S,
  125.0f, 0.0f, 0.0f,
  30U, 120U, 1U
};

static const WalkMetricsProfile run_profile = {
  RUN_MIN_STEP_INTERVAL_MS, RUN_MAX_STEP_INTERVAL_MS,
  RUN_GAIT_RESET_MS, RUN_STEP_EVENT_TIMEOUT_MS, RUN_PEAK_FALL_MIN_MS,
  RUN_DEFAULT_INTERVAL_MS, RUN_DEFAULT_STRIDE_M, RUN_MIN_STRIDE_M, RUN_MAX_STRIDE_M,
  RUN_ACCEL_MOTION_FLOOR_G, RUN_ACCEL_PEAK_FLOOR_G,
  RUN_GYRO_GATE_FLOOR_RAD_S, RUN_GYRO_PEAK_FLOOR_RAD_S,
  180.0f, 0.0f, 0.0f,
  30U, 120U, 0U
};

_Static_assert(sizeof(WalkMetricsRawDebugSample) == 16U,
               "Walk debug record layout must remain 16 bytes");
_Static_assert(sizeof(WalkMetricsDebugSession) == 20U,
               "Walk debug session layout must remain 20 bytes");
_Static_assert((WALK_METRICS_DEBUG_CAPACITY * sizeof(WalkMetricsRawDebugSample)) ==
                 EXT_FLASH_MOTION_DATA_BYTES,
               "Walk debug storage must match the motion Flash partition");
_Static_assert(sizeof(WalkMetricsFlashMetadata) <= EXT_FLASH_SECTOR_SIZE_BYTES,
               "Walk metadata snapshot must fit one Flash sector");
_Static_assert((EXT_FLASH_MOTION_DATA_BASE % EXT_FLASH_SECTOR_SIZE_BYTES) == 0UL,
               "Motion data partition must be sector aligned");
_Static_assert(EXT_FLASH_MOTION_DATA_END <= EXT_FLASH_SIZE_BYTES,
               "Motion data partition exceeds external Flash");

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

static const WalkMetricsProfile* profile_for_state(const WalkMetricsState* state)
{
  if ((state != NULL) && (state->mode == (uint8_t)WALK_METRICS_MODE_RUN))
  {
    return &run_profile;
  }
  if ((state != NULL) && (state->fast_walk != 0U))
  {
    return &fast_walk_profile;
  }
  return &walk_profile;
}

static float estimate_step_length_m(const WalkMetricsProfile* profile,
                                    uint32_t interval_ms, float peak_g)
{
  float cadence_spm;
  float stride = profile->default_stride_m;

  if ((interval_ms > 0U) && (interval_ms <= profile->max_step_interval_ms))
  {
    cadence_spm = 60000.0f / (float)interval_ms;
  }
  else
  {
    cadence_spm = profile->cadence_reference_spm;
  }

  stride += (cadence_spm - profile->cadence_reference_spm) *
            profile->cadence_stride_gain;
  stride += (peak_g - profile->accel_peak_floor_g) * profile->peak_stride_gain;
  return clampf(stride, profile->min_stride_m, profile->max_stride_m);
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

static uint8_t interval_is_consistent(const WalkMetricsProfile* profile,
                                      uint32_t interval_ms, uint32_t previous_ms)
{
  uint32_t difference;
  uint32_t tolerance;

  if ((interval_ms < profile->min_step_interval_ms) ||
      (interval_ms > profile->max_step_interval_ms) ||
      (previous_ms == 0U))
  {
    return (previous_ms == 0U) ? 1U : 0U;
  }

  difference = (interval_ms > previous_ms) ? (interval_ms - previous_ms) : (previous_ms - interval_ms);
  tolerance = previous_ms * profile->interval_tolerance_percent / 100U;
  if (tolerance < profile->interval_tolerance_floor_ms)
  {
    tolerance = profile->interval_tolerance_floor_ms;
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
  const WalkMetricsProfile* profile = profile_for_state(state);
  uint32_t interval_ms;
  float stride_m;

  if ((state->last_candidate_tick_ms == 0U) ||
      ((tick_ms - state->last_candidate_tick_ms) > profile->max_step_interval_ms))
  {
    start_candidate_chain(state, tick_ms, profile->default_interval_ms,
                          estimate_step_length_m(profile, profile->default_interval_ms,
                                                 peak_dynamic_g));
    return;
  }

  interval_ms = tick_ms - state->last_candidate_tick_ms;
  stride_m = estimate_step_length_m(profile, interval_ms, peak_dynamic_g);

  if (state->gait_confirmed != 0U)
  {
    if ((interval_ms >= profile->min_step_interval_ms) &&
        (interval_ms <= profile->max_step_interval_ms))
    {
      if ((profile->recover_missed_step != 0U) &&
          (state->last_candidate_interval_ms != 0U) &&
          (interval_ms > (state->last_candidate_interval_ms * 8U / 5U)) &&
          (interval_ms < (state->last_candidate_interval_ms * 5U / 2U)))
      {
        const uint32_t recovered_interval_ms = interval_ms / 2U;
        const float recovered_stride_m =
            estimate_step_length_m(profile, recovered_interval_ms, peak_dynamic_g);
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

    start_candidate_chain(state, tick_ms, profile->default_interval_ms,
                          estimate_step_length_m(profile, profile->default_interval_ms,
                                                 peak_dynamic_g));
    return;
  }

  if (interval_is_consistent(profile, interval_ms,
                             state->last_candidate_interval_ms) == 0U)
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

static uint32_t WalkFlash_Crc32(const uint8_t* data, uint32_t length)
{
  uint32_t crc = 0xFFFFFFFFUL;

  for (uint32_t index = 0UL; index < length; index++)
  {
    crc ^= data[index];
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      const uint32_t mask = (uint32_t)-(int32_t)(crc & 1UL);
      crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

static uint8_t WalkFlash_IsErased(const uint8_t* data, uint32_t length)
{
  for (uint32_t index = 0UL; index < length; index++)
  {
    if (data[index] != 0xFFU)
    {
      return 0U;
    }
  }
  return 1U;
}

static uint8_t WalkFlash_MetadataIsValid(const WalkMetricsFlashMetadata* metadata)
{
  uint32_t expected_crc;

  if ((metadata->magic != WALK_FLASH_META_MAGIC) ||
      (metadata->version != WALK_FLASH_META_VERSION) ||
      (metadata->write_index > WALK_METRICS_DEBUG_CAPACITY) ||
      (metadata->sample_count > metadata->write_index) ||
      (metadata->session_count > WALK_METRICS_DEBUG_MAX_SESSIONS) ||
      ((metadata->active_session_index != WALK_METRICS_DEBUG_NO_ACTIVE_SESSION) &&
       (metadata->active_session_index >= metadata->session_count)))
  {
    return 0U;
  }

  expected_crc = WalkFlash_Crc32((const uint8_t*)metadata,
                                 (uint32_t)offsetof(WalkMetricsFlashMetadata, crc32));
  return (expected_crc == metadata->crc32) ? 1U : 0U;
}

static uint8_t WalkFlash_ReadMetadata(uint32_t address,
                                      WalkMetricsFlashMetadata* metadata)
{
  if (ExtFlash_Read(address, (uint8_t*)metadata, sizeof(*metadata)) == 0U)
  {
    return 0U;
  }
  return WalkFlash_MetadataIsValid(metadata);
}

static void WalkFlash_BuildMetadata(uint32_t generation)
{
  memset(&walk_flash_metadata, 0, sizeof(walk_flash_metadata));
  walk_flash_metadata.magic = WALK_FLASH_META_MAGIC;
  walk_flash_metadata.version = WALK_FLASH_META_VERSION;
  walk_flash_metadata.generation = generation;
  walk_flash_metadata.write_index = g_walk_metrics_debug_write_index;
  walk_flash_metadata.sample_count = g_walk_metrics_debug_count;
  walk_flash_metadata.session_count = g_walk_metrics_debug_session_count;
  walk_flash_metadata.active_session_index = g_walk_metrics_debug_active_session_index;

  for (uint32_t index = 0UL; index < WALK_METRICS_DEBUG_MAX_SESSIONS; index++)
  {
    walk_flash_metadata.sessions[index].start_index =
      g_walk_metrics_debug_sessions[index].start_index;
    walk_flash_metadata.sessions[index].sample_count =
      g_walk_metrics_debug_sessions[index].sample_count;
    walk_flash_metadata.sessions[index].start_tick_ms =
      g_walk_metrics_debug_sessions[index].start_tick_ms;
    walk_flash_metadata.sessions[index].end_tick_ms =
      g_walk_metrics_debug_sessions[index].end_tick_ms;
    walk_flash_metadata.sessions[index].status =
      g_walk_metrics_debug_sessions[index].status;
  }

  walk_flash_metadata.crc32 = WalkFlash_Crc32(
    (const uint8_t*)&walk_flash_metadata,
    (uint32_t)offsetof(WalkMetricsFlashMetadata, crc32));
}

static uint8_t WalkFlash_PersistMetadata(void)
{
  const uint32_t generation = g_walk_metrics_flash_generation + 1UL;
  const uint32_t address = ((generation & 1UL) != 0UL) ?
    EXT_FLASH_MOTION_META_A_BASE : EXT_FLASH_MOTION_META_B_BASE;
  uint32_t offset = 0UL;

  WalkFlash_BuildMetadata(generation);
  if (ExtFlash_Erase4K(address) == 0U)
  {
    return 0U;
  }

  while (offset < sizeof(walk_flash_metadata))
  {
    uint32_t chunk = sizeof(walk_flash_metadata) - offset;
    if (chunk > EXT_FLASH_PAGE_SIZE_BYTES)
    {
      chunk = EXT_FLASH_PAGE_SIZE_BYTES;
    }
    if (ExtFlash_PageProgram(address + offset,
                             &((const uint8_t*)&walk_flash_metadata)[offset],
                             chunk) == 0U)
    {
      return 0U;
    }
    offset += chunk;
  }

  if ((ExtFlash_Read(address, (uint8_t*)&walk_flash_verify,
                     sizeof(walk_flash_verify)) == 0U) ||
      (WalkFlash_MetadataIsValid(&walk_flash_verify) == 0U) ||
      (walk_flash_verify.generation != generation))
  {
    return 0U;
  }

  g_walk_metrics_flash_generation = generation;
  return 1U;
}

static void WalkFlash_RestoreMetadata(const WalkMetricsFlashMetadata* metadata)
{
  g_walk_metrics_debug_write_index = metadata->write_index;
  g_walk_metrics_debug_count = metadata->sample_count;
  g_walk_metrics_debug_session_count = metadata->session_count;
  g_walk_metrics_debug_active_session_index = metadata->active_session_index;
  g_walk_metrics_flash_generation = metadata->generation;

  for (uint32_t index = 0UL; index < WALK_METRICS_DEBUG_MAX_SESSIONS; index++)
  {
    g_walk_metrics_debug_sessions[index].start_index =
      metadata->sessions[index].start_index;
    g_walk_metrics_debug_sessions[index].sample_count =
      metadata->sessions[index].sample_count;
    g_walk_metrics_debug_sessions[index].start_tick_ms =
      metadata->sessions[index].start_tick_ms;
    g_walk_metrics_debug_sessions[index].end_tick_ms =
      metadata->sessions[index].end_tick_ms;
    g_walk_metrics_debug_sessions[index].status =
      metadata->sessions[index].status;
  }
}

static uint8_t WalkFlash_RecoverInterruptedSession(void)
{
  const uint32_t session_index = g_walk_metrics_debug_active_session_index;
  uint32_t index;
  uint32_t recovered_count = 0UL;
  uint32_t last_tick_ms = 0UL;
  uint8_t finished = 0U;

  if ((session_index == WALK_METRICS_DEBUG_NO_ACTIVE_SESSION) ||
      (session_index >= g_walk_metrics_debug_session_count))
  {
    return 1U;
  }

  index = g_walk_metrics_debug_sessions[session_index].start_index;
  while ((index < WALK_METRICS_DEBUG_CAPACITY) && (finished == 0U))
  {
    uint32_t records = WALK_METRICS_DEBUG_EXPORT_BUFFER_BYTES /
                       sizeof(WalkMetricsRawDebugSample);
    const uint32_t remaining = WALK_METRICS_DEBUG_CAPACITY - index;
    if (records > remaining)
    {
      records = remaining;
    }

    if (ExtFlash_Read(EXT_FLASH_MOTION_DATA_BASE +
                      (index * sizeof(WalkMetricsRawDebugSample)),
                      g_walk_metrics_debug_export_buffer,
                      records * sizeof(WalkMetricsRawDebugSample)) == 0U)
    {
      return 0U;
    }

    for (uint32_t offset = 0UL; offset < records; offset++)
    {
      const uint8_t* bytes = &g_walk_metrics_debug_export_buffer[
        offset * sizeof(WalkMetricsRawDebugSample)];
      WalkMetricsRawDebugSample sample;

      if (WalkFlash_IsErased(bytes, sizeof(sample)) != 0U)
      {
        finished = 1U;
        break;
      }
      memcpy(&sample, bytes, sizeof(sample));
      last_tick_ms = sample.tick_ms;
      recovered_count++;
      index++;
    }
  }

  g_walk_metrics_debug_write_index = index;
  g_walk_metrics_debug_count = index;
  if (recovered_count == 0UL)
  {
    if (session_index == (g_walk_metrics_debug_session_count - 1UL))
    {
      memset((void*)&g_walk_metrics_debug_sessions[session_index], 0,
             sizeof(g_walk_metrics_debug_sessions[session_index]));
      g_walk_metrics_debug_session_count--;
    }
  }
  else
  {
    g_walk_metrics_debug_sessions[session_index].sample_count = recovered_count;
    g_walk_metrics_debug_sessions[session_index].end_tick_ms = last_tick_ms;
    g_walk_metrics_debug_sessions[session_index].status =
      WALK_METRICS_DEBUG_SESSION_COMPLETE;
  }
  g_walk_metrics_debug_active_session_index = WALK_METRICS_DEBUG_NO_ACTIVE_SESSION;
  g_walk_metrics_flash_recovery_count++;
  return WalkFlash_PersistMetadata();
}

static uint8_t WalkFlash_PrepareSector(uint32_t address)
{
  uint8_t first_record[sizeof(WalkMetricsRawDebugSample)];

  if (ExtFlash_Read(address, first_record, sizeof(first_record)) == 0U)
  {
    return 0U;
  }
  if (WalkFlash_IsErased(first_record, sizeof(first_record)) != 0U)
  {
    return 1U;
  }
  return ExtFlash_Erase4K(address);
}

static void WalkFlash_ClearRuntimeDirectory(void)
{
  g_walk_metrics_debug_write_index = 0UL;
  g_walk_metrics_debug_count = 0UL;
  g_walk_metrics_debug_error_count = 0UL;
  g_walk_metrics_debug_session_count = 0UL;
  g_walk_metrics_debug_active_session_index = WALK_METRICS_DEBUG_NO_ACTIVE_SESSION;
  memset((void*)g_walk_metrics_debug_sessions, 0,
         sizeof(g_walk_metrics_debug_sessions));
}

void WalkMetrics_DebugClear(void)
{
  WalkFlash_ClearRuntimeDirectory();
  if ((ExtFlash_IsReady() == 0U) || (WalkFlash_PersistMetadata() == 0U))
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
    g_walk_metrics_debug_error_count++;
    return;
  }
  g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_READY;
}

void WalkMetrics_DebugStorageInit(void)
{
  uint8_t valid_a;
  uint8_t valid_b;

  g_walk_metrics_debug_export_control.psram_address = 0UL;
  g_walk_metrics_debug_export_control.length = 0UL;
  g_walk_metrics_debug_export_control.request_id = 0UL;
  g_walk_metrics_debug_export_control.completed_id = 0UL;
  g_walk_metrics_debug_export_control.status = WALK_METRICS_DEBUG_EXPORT_IDLE;
  g_walk_metrics_flash_recovery_count = 0UL;

  if ((ExtFlash_IsReady() == 0U) && (ExtFlash_Init() == 0U))
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_NOT_READY;
    return;
  }

  valid_a = WalkFlash_ReadMetadata(EXT_FLASH_MOTION_META_A_BASE,
                                  &walk_flash_metadata);
  valid_b = WalkFlash_ReadMetadata(EXT_FLASH_MOTION_META_B_BASE,
                                  &walk_flash_verify);
  if ((valid_a != 0U) &&
      ((valid_b == 0U) ||
       (walk_flash_metadata.generation >= walk_flash_verify.generation)))
  {
    WalkFlash_RestoreMetadata(&walk_flash_metadata);
  }
  else if (valid_b != 0U)
  {
    WalkFlash_RestoreMetadata(&walk_flash_verify);
  }
  else
  {
    WalkFlash_ClearRuntimeDirectory();
    g_walk_metrics_flash_generation = 0UL;
    if (WalkFlash_PersistMetadata() == 0U)
    {
      g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
      g_walk_metrics_debug_error_count++;
      return;
    }
  }

  if (WalkFlash_RecoverInterruptedSession() == 0U)
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
    g_walk_metrics_debug_error_count++;
    return;
  }
  g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_READY;
}

void WalkMetrics_DebugBeginSession(uint32_t tick_ms)
{
  uint32_t session_index;
  volatile WalkMetricsDebugSession* session;

  if (g_walk_metrics_debug_active_session_index != WALK_METRICS_DEBUG_NO_ACTIVE_SESSION)
  {
    WalkMetrics_DebugEndSession(tick_ms);
  }

  if ((ExtFlash_IsReady() == 0U) ||
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

  if (WalkFlash_PersistMetadata() == 0U)
  {
    memset((void*)session, 0, sizeof(*session));
    g_walk_metrics_debug_session_count = session_index;
    g_walk_metrics_debug_active_session_index = WALK_METRICS_DEBUG_NO_ACTIVE_SESSION;
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
    g_walk_metrics_debug_error_count++;
  }
}

void WalkMetrics_DebugEndSession(uint32_t tick_ms)
{
  const uint32_t session_index = g_walk_metrics_debug_active_session_index;

  if ((session_index == WALK_METRICS_DEBUG_NO_ACTIVE_SESSION) ||
      (session_index >= g_walk_metrics_debug_session_count))
  {
    return;
  }

  g_walk_metrics_debug_sessions[session_index].end_tick_ms = tick_ms;
  g_walk_metrics_debug_sessions[session_index].status =
    WALK_METRICS_DEBUG_SESSION_COMPLETE;
  g_walk_metrics_debug_active_session_index = WALK_METRICS_DEBUG_NO_ACTIVE_SESSION;
  if (WalkFlash_PersistMetadata() == 0U)
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
    g_walk_metrics_debug_error_count++;
  }
}

void WalkMetrics_DebugServiceExport(void)
{
  const uint32_t request_id = g_walk_metrics_debug_export_control.request_id;
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

  if ((address == WALK_METRICS_DEBUG_CLEAR_COMMAND) && (length == 0UL))
  {
    WalkMetrics_DebugClear();
    success = (g_walk_metrics_debug_storage_status ==
               WALK_METRICS_DEBUG_STORAGE_READY) ? 1U : 0U;
  }
  else if ((ExtFlash_IsReady() != 0U) &&
      (length > 0UL) &&
      (length <= WALK_METRICS_DEBUG_EXPORT_BUFFER_BYTES) &&
      (address < EXT_FLASH_MOTION_DATA_BYTES) &&
      ((EXT_FLASH_MOTION_DATA_BYTES - address) >= length))
  {
    success = ExtFlash_Read(EXT_FLASH_MOTION_DATA_BASE + address,
                            g_walk_metrics_debug_export_buffer, length);
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
  uint32_t address;

  if ((accel_raw == NULL) || (gyro_raw == NULL))
  {
    return;
  }

  if (ExtFlash_IsReady() == 0U)
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

  address = EXT_FLASH_MOTION_DATA_BASE + (index * sizeof(sample));
  if (((index % WALK_FLASH_RECORDS_PER_SECTOR) == 0UL) &&
      (WalkFlash_PrepareSector(address) == 0U))
  {
    g_walk_metrics_debug_storage_status = WALK_METRICS_DEBUG_STORAGE_ERROR;
    g_walk_metrics_debug_error_count++;
    return;
  }

  if (((index % WALK_FLASH_RECORDS_PER_SECTOR) ==
       (WALK_FLASH_RECORDS_PER_SECTOR - 1UL)) &&
      ((index + 1UL) < WALK_METRICS_DEBUG_CAPACITY) &&
      (WalkFlash_PrepareSector(address + sizeof(sample)) == 0U))
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

  if (ExtFlash_PageProgram(address, (const uint8_t*)&sample,
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
  state->mode = (uint8_t)WALK_METRICS_MODE_WALK;
  state->gravity_g = 1.0f;
  state->noise_dynamic_g = 0.025f;
  state->noise_gyro_rad_s = 0.04f;
  state->last_step_length_m = WALK_DEFAULT_STRIDE_M;
  state->average_step_length_m = WALK_DEFAULT_STRIDE_M;
}

void WalkMetrics_Start(WalkMetricsState* state, uint32_t tick_ms, WalkMetricsMode mode)
{
  const WalkMetricsProfile* profile;

  WalkMetrics_Reset(state);
  if (state == NULL)
  {
    return;
  }

  state->mode = (mode == WALK_METRICS_MODE_RUN)
                  ? (uint8_t)WALK_METRICS_MODE_RUN
                  : (uint8_t)WALK_METRICS_MODE_WALK;
  profile = profile_for_state(state);
  state->last_step_length_m = profile->default_stride_m;
  state->average_step_length_m = profile->default_stride_m;
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

void WalkMetrics_SetFastWalk(WalkMetricsState* state, uint8_t fast_walk)
{
  const uint8_t requested = (fast_walk != 0U) ? 1U : 0U;

  if ((state == NULL) || (state->mode == (uint8_t)WALK_METRICS_MODE_RUN) ||
      (state->fast_walk == requested))
  {
    return;
  }

  state->fast_walk = requested;
  clear_candidate_chain(state);
  state->step_active = 0U;
  state->step_start_tick_ms = 0U;
  state->peak_tick_ms = 0U;
  state->peak_dynamic_g = 0.0f;
  state->peak_gyro_norm_rad_s = 0.0f;
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
  const WalkMetricsProfile* profile;
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

  profile = profile_for_state(state);

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

  accel_motion_threshold = maxf(profile->accel_motion_floor_g,
                                state->noise_dynamic_g * 3.0f);
  accel_peak_threshold = maxf(profile->accel_peak_floor_g,
                              state->noise_dynamic_g * 5.0f);
  gyro_gate_threshold = maxf(profile->gyro_gate_floor_rad_s,
                             state->noise_gyro_rad_s * 3.0f);
  gyro_peak_threshold = maxf(profile->gyro_peak_floor_rad_s,
                             state->noise_gyro_rad_s * 5.0f);

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
      ((sample->tick_ms - state->last_candidate_tick_ms) > profile->gait_reset_ms))
  {
    clear_candidate_chain(state);
  }

  strong_motion_event = (((abs_dynamic_g >= accel_peak_threshold) &&
                          (state->filtered_gyro_rad_s >= gyro_gate_threshold)) ||
                         ((state->filtered_gyro_rad_s >= gyro_peak_threshold) &&
                          (abs_dynamic_g >= accel_motion_threshold))) ? 1U : 0U;

  if ((state->step_active == 0U) && (strong_motion_event != 0U) &&
      ((state->last_candidate_tick_ms == 0U) ||
       ((sample->tick_ms - state->last_candidate_tick_ms) >=
        profile->min_step_interval_ms)))
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

    if ((((sample->tick_ms - state->step_start_tick_ms) >= profile->peak_fall_min_ms) &&
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
    else if ((sample->tick_ms - state->step_start_tick_ms) >
             profile->step_event_timeout_ms)
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
