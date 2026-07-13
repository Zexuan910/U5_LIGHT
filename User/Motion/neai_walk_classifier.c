#include "neai_walk_classifier.h"

#include <stddef.h>
#include <string.h>

#include "NanoEdgeAI.h"

#define NEAI_WALK_AXIS_COUNT 6U
#define NEAI_WALK_WINDOW_SAMPLES 64U
#define NEAI_WALK_SIGNAL_VALUES (NEAI_WALK_AXIS_COUNT * NEAI_WALK_WINDOW_SAMPLES)
#define NEAI_WALK_STRIDE_SAMPLES 16U
#define NEAI_WALK_CONFIDENCE_THRESHOLD 0.65f
#define NEAI_WALK_CONFIRMATIONS 2U

#define NEAI_STATUS_NOT_INITIALIZED 0x4E450000UL
#define NEAI_STATUS_OK 0x4E454F4BUL
#define NEAI_STATUS_MODEL_ERROR 0x4E454552UL
#define NEAI_STATUS_CONFIG_ERROR 0x4E454346UL

_Static_assert(NEAI_INPUT_SIGNAL_LENGTH == NEAI_WALK_WINDOW_SAMPLES,
               "NanoEdge AI signal length mismatch");
_Static_assert(NEAI_INPUT_AXIS_NUMBER == NEAI_WALK_AXIS_COUNT,
               "NanoEdge AI axis count mismatch");
_Static_assert(NEAI_NUMBER_OF_CLASSES == 2,
               "NanoEdge AI class count mismatch");

volatile uint32_t g_neai_status = NEAI_STATUS_NOT_INITIALIZED;
volatile uint32_t g_neai_inference_count = 0U;
volatile uint16_t g_neai_walk_probability_x1000 = 0U;
volatile uint16_t g_neai_still_probability_x1000 = 0U;
volatile uint16_t g_neai_window_samples = 0U;
volatile int8_t g_neai_predicted_class = -1;
volatile int8_t g_neai_walk_class_id = -1;
volatile uint8_t g_neai_last_state = (uint8_t)NEAI_NOT_INITIALIZED;
volatile uint8_t g_neai_model_ready = 0U;
volatile uint8_t g_neai_has_prediction = 0U;
volatile uint8_t g_neai_walk_active = 1U;

static float neai_sample_window[NEAI_WALK_SIGNAL_VALUES];
static float neai_inference_input[NEAI_WALK_SIGNAL_VALUES];
static float neai_probabilities[NEAI_NUMBER_OF_CLASSES];
static uint16_t neai_sample_count = 0U;
static uint16_t neai_samples_since_inference = 0U;
static uint8_t neai_walk_streak = 0U;
static uint8_t neai_still_streak = 0U;
static int8_t neai_still_class_id = -1;

static uint16_t NeaiWalkClassifier_ProbabilityX1000(float probability)
{
  if (probability <= 0.0f)
  {
    return 0U;
  }
  if (probability >= 1.0f)
  {
    return 1000U;
  }
  return (uint16_t)(probability * 1000.0f + 0.5f);
}

static void NeaiWalkClassifier_RunInference(void)
{
  enum neai_state state;
  int predicted_class = -1;
  float walk_probability;
  uint8_t raw_walk;

  memcpy(neai_inference_input, neai_sample_window, sizeof(neai_inference_input));
  state = neai_classification(neai_inference_input, neai_probabilities, &predicted_class);
  g_neai_last_state = (uint8_t)state;
  g_neai_inference_count++;
  if (state != NEAI_OK)
  {
    g_neai_status = NEAI_STATUS_MODEL_ERROR;
    g_neai_model_ready = 0U;
    g_neai_walk_active = 1U;
    return;
  }

  g_neai_predicted_class = (int8_t)predicted_class;
  g_neai_has_prediction = 1U;
  walk_probability = neai_probabilities[(uint8_t)g_neai_walk_class_id];
  g_neai_walk_probability_x1000 = NeaiWalkClassifier_ProbabilityX1000(walk_probability);
  g_neai_still_probability_x1000 =
      NeaiWalkClassifier_ProbabilityX1000(neai_probabilities[(uint8_t)neai_still_class_id]);

  raw_walk = ((predicted_class == g_neai_walk_class_id) &&
              (walk_probability >= NEAI_WALK_CONFIDENCE_THRESHOLD)) ? 1U : 0U;
  if (raw_walk != 0U)
  {
    neai_still_streak = 0U;
    if (neai_walk_streak < NEAI_WALK_CONFIRMATIONS)
    {
      neai_walk_streak++;
    }
    if (neai_walk_streak >= NEAI_WALK_CONFIRMATIONS)
    {
      g_neai_walk_active = 1U;
    }
  }
  else
  {
    neai_walk_streak = 0U;
    if (neai_still_streak < NEAI_WALK_CONFIRMATIONS)
    {
      neai_still_streak++;
    }
    if (neai_still_streak >= NEAI_WALK_CONFIRMATIONS)
    {
      g_neai_walk_active = 0U;
    }
  }

  g_neai_status = NEAI_STATUS_OK;
}

bool NeaiWalkClassifier_Init(void)
{
  enum neai_state state;

  g_neai_status = NEAI_STATUS_NOT_INITIALIZED;
  g_neai_model_ready = 0U;
  g_neai_walk_class_id = -1;
  neai_still_class_id = -1;

  if ((neai_get_input_signal_size() != (int)NEAI_WALK_WINDOW_SAMPLES) ||
      (neai_get_axis_number() != (int)NEAI_WALK_AXIS_COUNT) ||
      (neai_get_number_of_classes() != (int)NEAI_NUMBER_OF_CLASSES))
  {
    g_neai_status = NEAI_STATUS_CONFIG_ERROR;
    return false;
  }

  for (int class_id = 0; class_id < (int)NEAI_NUMBER_OF_CLASSES; class_id++)
  {
    const char* class_name = neai_get_class_name(class_id);

    if ((class_name != NULL) && (strcmp(class_name, "normal_walk") == 0))
    {
      g_neai_walk_class_id = (int8_t)class_id;
    }
    else if ((class_name != NULL) && (strcmp(class_name, "still") == 0))
    {
      neai_still_class_id = (int8_t)class_id;
    }
  }

  if ((g_neai_walk_class_id < 0) || (neai_still_class_id < 0))
  {
    g_neai_status = NEAI_STATUS_CONFIG_ERROR;
    return false;
  }

  state = neai_classification_init();
  g_neai_last_state = (uint8_t)state;
  if (state != NEAI_OK)
  {
    g_neai_status = NEAI_STATUS_MODEL_ERROR;
    return false;
  }

  g_neai_model_ready = 1U;
  g_neai_status = NEAI_STATUS_OK;
  NeaiWalkClassifier_Reset();
  return true;
}

void NeaiWalkClassifier_Reset(void)
{
  memset(neai_sample_window, 0, sizeof(neai_sample_window));
  memset(neai_inference_input, 0, sizeof(neai_inference_input));
  memset(neai_probabilities, 0, sizeof(neai_probabilities));
  neai_sample_count = 0U;
  neai_samples_since_inference = 0U;
  neai_walk_streak = 0U;
  neai_still_streak = 0U;
  g_neai_inference_count = 0U;
  g_neai_walk_probability_x1000 = 0U;
  g_neai_still_probability_x1000 = 0U;
  g_neai_window_samples = 0U;
  g_neai_predicted_class = -1;
  g_neai_has_prediction = 0U;
  g_neai_walk_active = 1U;
}

void NeaiWalkClassifier_PushSample(const float accel_g[3], const float gyro_rad_s[3])
{
  uint16_t base;

  if ((g_neai_model_ready == 0U) || (accel_g == NULL) || (gyro_rad_s == NULL))
  {
    return;
  }

  if (neai_sample_count < NEAI_WALK_WINDOW_SAMPLES)
  {
    base = neai_sample_count * NEAI_WALK_AXIS_COUNT;
    neai_sample_count++;
    g_neai_window_samples = neai_sample_count;
  }
  else
  {
    memmove(neai_sample_window,
            &neai_sample_window[NEAI_WALK_AXIS_COUNT],
            (NEAI_WALK_SIGNAL_VALUES - NEAI_WALK_AXIS_COUNT) * sizeof(float));
    base = NEAI_WALK_SIGNAL_VALUES - NEAI_WALK_AXIS_COUNT;
    neai_samples_since_inference++;
  }

  for (uint8_t axis = 0U; axis < 3U; axis++)
  {
    neai_sample_window[base + axis] = accel_g[axis];
    neai_sample_window[base + 3U + axis] = gyro_rad_s[axis];
  }

  if (neai_sample_count == NEAI_WALK_WINDOW_SAMPLES)
  {
    if ((g_neai_has_prediction == 0U) ||
        (neai_samples_since_inference >= NEAI_WALK_STRIDE_SAMPLES))
    {
      neai_samples_since_inference = 0U;
      NeaiWalkClassifier_RunInference();
    }
  }
}

bool NeaiWalkClassifier_AllowStepDetection(void)
{
  if ((g_neai_model_ready == 0U) || (g_neai_has_prediction == 0U))
  {
    return true;
  }
  return g_neai_walk_active != 0U;
}

uint8_t NeaiWalkClassifier_GetUiState(void)
{
  if (g_neai_model_ready == 0U)
  {
    return (uint8_t)NEAI_WALK_UI_ERROR;
  }
  if (g_neai_has_prediction == 0U)
  {
    return (uint8_t)NEAI_WALK_UI_WAIT;
  }
  return (g_neai_walk_active != 0U) ? (uint8_t)NEAI_WALK_UI_WALK
                                    : (uint8_t)NEAI_WALK_UI_STILL;
}
