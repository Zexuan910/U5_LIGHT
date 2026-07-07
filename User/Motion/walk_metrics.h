#ifndef WALK_METRICS_H
#define WALK_METRICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t tick_ms;
  float accel_g[3];
  float gyro_rad_s[3];
} WalkMetricsImuSample;

typedef struct {
  uint32_t elapsed_ms;
  uint32_t step_count;
  float instant_speed_mps;
  float average_speed_mps;
  float stride_m;
  float distance_m;
  uint8_t output_updated;
} WalkMetricsOutput;

typedef struct {
  uint8_t running;
  uint8_t step_active;
  uint32_t start_tick_ms;
  uint32_t last_sample_tick_ms;
  uint32_t last_step_tick_ms;
  uint32_t last_step_interval_ms;
  uint32_t last_output_tick_ms;
  uint32_t last_motion_tick_ms;
  uint32_t step_count;
  float gravity_g;
  float prev_dynamic_g;
  float prev_gyro_norm_rad_s;
  float peak_dynamic_g;
  float peak_gyro_norm_rad_s;
  float last_step_length_m;
  float average_step_length_m;
  float instant_speed_mps;
  float distance_m;
  float window_distance_m;
  uint32_t window_start_tick_ms;
} WalkMetricsState;

void WalkMetrics_Reset(WalkMetricsState *state);
void WalkMetrics_Start(WalkMetricsState *state, uint32_t tick_ms);
void WalkMetrics_Stop(WalkMetricsState *state);
void WalkMetrics_Update(WalkMetricsState *state, const WalkMetricsImuSample *sample, WalkMetricsOutput *output);

#ifdef __cplusplus
}
#endif

#endif
