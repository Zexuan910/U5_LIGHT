#ifndef NEAI_RUN_CLASSIFIER_H
#define NEAI_RUN_CLASSIFIER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  NEAI_RUN_UI_WAIT = 0,
  NEAI_RUN_UI_RUN,
  NEAI_RUN_UI_STILL,
  NEAI_RUN_UI_ERROR
} NeaiRunUiState;

extern volatile uint32_t g_neai_run_status;
extern volatile uint32_t g_neai_run_inference_count;
extern volatile uint16_t g_neai_run_probability_x1000;
extern volatile uint16_t g_neai_run_still_probability_x1000;
extern volatile uint16_t g_neai_run_window_samples;
extern volatile int8_t g_neai_run_predicted_class;
extern volatile int8_t g_neai_run_class_id;
extern volatile int8_t g_neai_run_still_class_id;
extern volatile uint8_t g_neai_run_last_state;
extern volatile uint8_t g_neai_run_model_ready;
extern volatile uint8_t g_neai_run_has_prediction;
extern volatile uint8_t g_neai_run_active;

bool NeaiRunClassifier_Init(void);
void NeaiRunClassifier_Reset(void);
void NeaiRunClassifier_PushSample(const float accel_g[3],
                                  const float gyro_rad_s[3]);
bool NeaiRunClassifier_AllowMetrics(void);
uint8_t NeaiRunClassifier_GetUiState(void);

#ifdef __cplusplus
}
#endif

#endif
