#ifndef ROPE_METRICS_H
#define ROPE_METRICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint32_t tick_ms;
  float accel_g[3];
  float gyro_rad_s[3];
} RopeMetricsImuSample;

typedef struct
{
  uint32_t elapsed_ms;
  uint32_t jump_count;
  uint16_t current_rate_x10;
  uint16_t average_rate_x10;
  uint16_t last_peak_gyro_x10;
  uint8_t output_updated;
} RopeMetricsOutput;

typedef struct
{
  uint8_t running;
  uint8_t event_active;
  uint32_t start_tick_ms;
  uint32_t last_sample_tick_ms;
  uint32_t event_start_tick_ms;
  uint32_t last_interval_ms;
  uint32_t last_count_tick_ms;
  uint32_t jump_count;
  float gravity_g;
  float filtered_dynamic_g;
  float filtered_rotation_rad_s;
  float noise_dynamic_g;
  float noise_rotation_rad_s;
  float event_peak_dynamic_g;
  float event_peak_rotation_rad_s;
  float smoothed_rate_x10;
} RopeMetricsState;

void RopeMetrics_Reset(RopeMetricsState* state);
void RopeMetrics_Start(RopeMetricsState* state, uint32_t tick_ms);
void RopeMetrics_Stop(RopeMetricsState* state);
void RopeMetrics_SuppressMotion(RopeMetricsState* state, uint32_t tick_ms,
                                RopeMetricsOutput* output);
void RopeMetrics_Update(RopeMetricsState* state,
                        const RopeMetricsImuSample* sample,
                        RopeMetricsOutput* output);

#ifdef __cplusplus
}
#endif

#endif
