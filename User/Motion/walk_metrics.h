#ifndef WALK_METRICS_H
#define WALK_METRICS_H

#include <stdint.h>

#define WALK_METRICS_DEBUG_CAPACITY            524288UL
#define WALK_METRICS_DEBUG_EXPORT_BUFFER_BYTES 65536UL
#define WALK_METRICS_DEBUG_MAX_SESSIONS        64U
#define WALK_METRICS_DEBUG_NO_ACTIVE_SESSION   0xFFFFFFFFUL

#define WALK_METRICS_DEBUG_STORAGE_NOT_READY 0U
#define WALK_METRICS_DEBUG_STORAGE_READY     1U
#define WALK_METRICS_DEBUG_STORAGE_ERROR     2U

#define WALK_METRICS_DEBUG_EXPORT_IDLE  0U
#define WALK_METRICS_DEBUG_EXPORT_BUSY  1U
#define WALK_METRICS_DEBUG_EXPORT_READY 2U
#define WALK_METRICS_DEBUG_EXPORT_ERROR 3U

#define WALK_METRICS_DEBUG_SESSION_ACTIVE   1U
#define WALK_METRICS_DEBUG_SESSION_COMPLETE 2U

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint32_t tick_ms;
  float accel_g[3];
  float gyro_rad_s[3];
} WalkMetricsImuSample;

typedef struct
{
  uint32_t elapsed_ms;
  uint32_t step_count;
  float instant_speed_mps;
  float average_speed_mps;
  float stride_m;
  float distance_m;
  uint8_t output_updated;
} WalkMetricsOutput;

typedef struct
{
  uint32_t tick_ms;
  int16_t accel_raw[3];
  int16_t gyro_raw[3];
} WalkMetricsRawDebugSample;

typedef struct
{
  uint32_t psram_address;
  uint32_t length;
  uint32_t request_id;
  uint32_t completed_id;
  uint32_t status;
} WalkMetricsDebugExportControl;

typedef struct
{
  uint32_t start_index;
  uint32_t sample_count;
  uint32_t start_tick_ms;
  uint32_t end_tick_ms;
  uint32_t status;
} WalkMetricsDebugSession;

typedef struct
{
  uint8_t running;
  uint8_t step_active;
  uint8_t gait_confirmed;
  uint8_t candidate_count;
  uint32_t start_tick_ms;
  uint32_t last_sample_tick_ms;
  uint32_t last_step_tick_ms;
  uint32_t last_candidate_tick_ms;
  uint32_t last_candidate_interval_ms;
  uint32_t last_step_interval_ms;
  uint32_t last_output_tick_ms;
  uint32_t last_motion_tick_ms;
  uint32_t step_start_tick_ms;
  uint32_t peak_tick_ms;
  uint32_t step_count;
  float gravity_g;
  float filtered_dynamic_g;
  float filtered_gyro_rad_s;
  float noise_dynamic_g;
  float noise_gyro_rad_s;
  float prev_dynamic_g;
  float prev_gyro_norm_rad_s;
  float peak_dynamic_g;
  float peak_gyro_norm_rad_s;
  float candidate_stride_m[3];
  uint32_t candidate_interval_ms[3];
  float last_step_length_m;
  float average_step_length_m;
  float instant_speed_mps;
  float distance_m;
} WalkMetricsState;

extern volatile uint32_t g_walk_metrics_debug_write_index;
extern volatile uint32_t g_walk_metrics_debug_count;
extern volatile uint32_t g_walk_metrics_debug_storage_status;
extern volatile uint32_t g_walk_metrics_debug_error_count;
extern volatile uint32_t g_walk_metrics_debug_session_count;
extern volatile uint32_t g_walk_metrics_debug_active_session_index;
extern volatile WalkMetricsDebugSession
  g_walk_metrics_debug_sessions[WALK_METRICS_DEBUG_MAX_SESSIONS];
extern volatile WalkMetricsDebugExportControl g_walk_metrics_debug_export_control;
extern uint8_t g_walk_metrics_debug_export_buffer[WALK_METRICS_DEBUG_EXPORT_BUFFER_BYTES];

void WalkMetrics_Reset(WalkMetricsState* state);
void WalkMetrics_Start(WalkMetricsState* state, uint32_t tick_ms);
void WalkMetrics_Stop(WalkMetricsState* state);
void WalkMetrics_SuppressMotion(WalkMetricsState* state, uint32_t tick_ms,
                                WalkMetricsOutput* output);
void WalkMetrics_Update(WalkMetricsState* state, const WalkMetricsImuSample* sample, WalkMetricsOutput* output);
void WalkMetrics_DebugClear(void);
void WalkMetrics_DebugStorageInit(void);
void WalkMetrics_DebugBeginSession(uint32_t tick_ms);
void WalkMetrics_DebugEndSession(uint32_t tick_ms);
void WalkMetrics_DebugServiceExport(void);
void WalkMetrics_DebugRecordRaw(uint32_t tick_ms, const int16_t accel_raw[3], const int16_t gyro_raw[3]);

#ifdef __cplusplus
}
#endif

#endif
