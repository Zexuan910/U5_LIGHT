#ifndef NEAI_WALK_CLASSIFIER_H
#define NEAI_WALK_CLASSIFIER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  NEAI_WALK_UI_WAIT = 0,
  NEAI_WALK_UI_WALK,
  NEAI_WALK_UI_STILL,
  NEAI_WALK_UI_ERROR
} NeaiWalkUiState;

extern volatile uint32_t g_neai_status;
extern volatile uint32_t g_neai_inference_count;
extern volatile uint16_t g_neai_walk_probability_x1000;
extern volatile uint16_t g_neai_still_probability_x1000;
extern volatile uint16_t g_neai_window_samples;
extern volatile int8_t g_neai_predicted_class;
extern volatile int8_t g_neai_walk_class_id;
extern volatile uint8_t g_neai_last_state;
extern volatile uint8_t g_neai_model_ready;
extern volatile uint8_t g_neai_has_prediction;
extern volatile uint8_t g_neai_walk_active;

bool NeaiWalkClassifier_Init(void);
void NeaiWalkClassifier_Reset(void);
void NeaiWalkClassifier_PushSample(const float accel_g[3], const float gyro_rad_s[3]);
bool NeaiWalkClassifier_AllowStepDetection(void);
uint8_t NeaiWalkClassifier_GetUiState(void);

#ifdef __cplusplus
}
#endif

#endif
