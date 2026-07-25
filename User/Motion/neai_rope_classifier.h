#ifndef NEAI_ROPE_CLASSIFIER_H
#define NEAI_ROPE_CLASSIFIER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  NEAI_ROPE_UI_WAIT = 0,
  NEAI_ROPE_UI_ROPE,
  NEAI_ROPE_UI_STILL,
  NEAI_ROPE_UI_ERROR
} NeaiRopeUiState;

extern volatile uint32_t g_neai_rope_status;
extern volatile uint32_t g_neai_rope_inference_count;
extern volatile uint16_t g_neai_rope_probability_x1000;
extern volatile uint16_t g_neai_rope_still_probability_x1000;
extern volatile uint16_t g_neai_rope_window_samples;
extern volatile int8_t g_neai_rope_predicted_class;
extern volatile int8_t g_neai_rope_class_id;
extern volatile int8_t g_neai_rope_still_class_id;
extern volatile uint8_t g_neai_rope_last_state;
extern volatile uint8_t g_neai_rope_model_ready;
extern volatile uint8_t g_neai_rope_has_prediction;
extern volatile uint8_t g_neai_rope_active;

bool NeaiRopeClassifier_Init(void);
void NeaiRopeClassifier_Reset(void);
void NeaiRopeClassifier_PushSample(const float accel_g[3],
                                   const float gyro_rad_s[3]);
bool NeaiRopeClassifier_AllowMetrics(void);
uint8_t NeaiRopeClassifier_GetUiState(void);

#ifdef __cplusplus
}
#endif

#endif
