#include "neai_rope_classifier.h"

#include <stddef.h>
#include <string.h>

#include "NanoEdgeAI_Rope.h"

#define NEAI_ROPE_AXIS_COUNT 6U
#define NEAI_ROPE_WINDOW_SAMPLES 64U
#define NEAI_ROPE_SIGNAL_VALUES \
  (NEAI_ROPE_AXIS_COUNT * NEAI_ROPE_WINDOW_SAMPLES)
#define NEAI_ROPE_STRIDE_SAMPLES 16U
#define NEAI_ROPE_CONFIDENCE_THRESHOLD 0.65f
#define NEAI_ROPE_CONFIRMATIONS 2U

#define NEAI_ROPE_STATUS_NOT_INITIALIZED 0x4E500000UL
#define NEAI_ROPE_STATUS_OK 0x4E504F4BUL
#define NEAI_ROPE_STATUS_MODEL_ERROR 0x4E504552UL
#define NEAI_ROPE_STATUS_CONFIG_ERROR 0x4E504346UL

_Static_assert(ROPE_NEAI_INPUT_SIGNAL_LENGTH == NEAI_ROPE_WINDOW_SAMPLES,
               "ROPE NanoEdge signal length mismatch");
_Static_assert(ROPE_NEAI_INPUT_AXIS_NUMBER == NEAI_ROPE_AXIS_COUNT,
               "ROPE NanoEdge axis count mismatch");
_Static_assert(ROPE_NEAI_NUMBER_OF_CLASSES == 2,
               "ROPE NanoEdge class count mismatch");

volatile uint32_t g_neai_rope_status = NEAI_ROPE_STATUS_NOT_INITIALIZED;
volatile uint32_t g_neai_rope_inference_count = 0U;
volatile uint16_t g_neai_rope_probability_x1000 = 0U;
volatile uint16_t g_neai_rope_still_probability_x1000 = 0U;
volatile uint16_t g_neai_rope_window_samples = 0U;
volatile int8_t g_neai_rope_predicted_class = -1;
volatile int8_t g_neai_rope_class_id = -1;
volatile int8_t g_neai_rope_still_class_id = -1;
volatile uint8_t g_neai_rope_last_state =
  (uint8_t)ROPE_NEAI_NOT_INITIALIZED;
volatile uint8_t g_neai_rope_model_ready = 0U;
volatile uint8_t g_neai_rope_has_prediction = 0U;
volatile uint8_t g_neai_rope_active = 0U;

static float neai_rope_sample_window[NEAI_ROPE_SIGNAL_VALUES];
static float neai_rope_inference_input[NEAI_ROPE_SIGNAL_VALUES];
static float neai_rope_probabilities[ROPE_NEAI_NUMBER_OF_CLASSES];
static uint16_t neai_rope_sample_count = 0U;
static uint16_t neai_rope_samples_since_inference = 0U;
static uint8_t neai_rope_streak = 0U;
static uint8_t neai_rope_still_streak = 0U;

static uint8_t NeaiRopeClassifier_NameStartsWith(const char* name,
                                                const char* prefix)
{
  size_t prefix_length;

  if ((name == NULL) || (prefix == NULL))
  {
    return 0U;
  }
  prefix_length = strlen(prefix);
  return (strncmp(name, prefix, prefix_length) == 0) ? 1U : 0U;
}

static uint16_t NeaiRopeClassifier_ProbabilityX1000(float probability)
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

static void NeaiRopeClassifier_RunInference(void)
{
  RopeNeaiState state;
  int predicted_class = -1;
  float predicted_probability;
  uint8_t raw_rope;

  memcpy(neai_rope_inference_input, neai_rope_sample_window,
         sizeof(neai_rope_inference_input));
  state = ropemodel_neai_classification(neai_rope_inference_input,
                                        neai_rope_probabilities,
                                        &predicted_class);
  g_neai_rope_last_state = (uint8_t)state;
  g_neai_rope_inference_count++;
  if ((state != ROPE_NEAI_OK) || (predicted_class < 0) ||
      (predicted_class >= (int)ROPE_NEAI_NUMBER_OF_CLASSES))
  {
    g_neai_rope_status = NEAI_ROPE_STATUS_MODEL_ERROR;
    g_neai_rope_model_ready = 0U;
    g_neai_rope_active = 0U;
    return;
  }

  g_neai_rope_predicted_class = (int8_t)predicted_class;
  g_neai_rope_has_prediction = 1U;
  g_neai_rope_probability_x1000 =
    NeaiRopeClassifier_ProbabilityX1000(
      neai_rope_probabilities[(uint8_t)g_neai_rope_class_id]);
  g_neai_rope_still_probability_x1000 =
    NeaiRopeClassifier_ProbabilityX1000(
      neai_rope_probabilities[(uint8_t)g_neai_rope_still_class_id]);
  predicted_probability = neai_rope_probabilities[(uint8_t)predicted_class];
  raw_rope = ((predicted_class == g_neai_rope_class_id) &&
              (predicted_probability >=
               NEAI_ROPE_CONFIDENCE_THRESHOLD)) ? 1U : 0U;

  if (raw_rope != 0U)
  {
    neai_rope_still_streak = 0U;
    if (neai_rope_streak < NEAI_ROPE_CONFIRMATIONS)
    {
      neai_rope_streak++;
    }
    if (neai_rope_streak >= NEAI_ROPE_CONFIRMATIONS)
    {
      g_neai_rope_active = 1U;
    }
  }
  else
  {
    neai_rope_streak = 0U;
    if (neai_rope_still_streak < NEAI_ROPE_CONFIRMATIONS)
    {
      neai_rope_still_streak++;
    }
    if (neai_rope_still_streak >= NEAI_ROPE_CONFIRMATIONS)
    {
      g_neai_rope_active = 0U;
    }
  }
  g_neai_rope_status = NEAI_ROPE_STATUS_OK;
}

bool NeaiRopeClassifier_Init(void)
{
  RopeNeaiState state;

  g_neai_rope_status = NEAI_ROPE_STATUS_NOT_INITIALIZED;
  g_neai_rope_model_ready = 0U;
  g_neai_rope_class_id = -1;
  g_neai_rope_still_class_id = -1;

  if ((ropemodel_neai_get_input_signal_size() !=
       (int)NEAI_ROPE_WINDOW_SAMPLES) ||
      (ropemodel_neai_get_axis_number() !=
       (int)NEAI_ROPE_AXIS_COUNT) ||
      (ropemodel_neai_get_number_of_classes() !=
       (int)ROPE_NEAI_NUMBER_OF_CLASSES))
  {
    g_neai_rope_status = NEAI_ROPE_STATUS_CONFIG_ERROR;
    return false;
  }

  for (int class_id = 0;
       class_id < (int)ROPE_NEAI_NUMBER_OF_CLASSES;
       class_id++)
  {
    const char* class_name = ropemodel_neai_get_class_name(class_id);

    if (NeaiRopeClassifier_NameStartsWith(class_name, "rope") != 0U)
    {
      g_neai_rope_class_id = (int8_t)class_id;
    }
    else if (NeaiRopeClassifier_NameStartsWith(class_name, "still") != 0U)
    {
      g_neai_rope_still_class_id = (int8_t)class_id;
    }
  }
  if ((g_neai_rope_class_id < 0) || (g_neai_rope_still_class_id < 0))
  {
    g_neai_rope_status = NEAI_ROPE_STATUS_CONFIG_ERROR;
    return false;
  }

  state = ropemodel_neai_classification_init();
  g_neai_rope_last_state = (uint8_t)state;
  if (state != ROPE_NEAI_OK)
  {
    g_neai_rope_status = NEAI_ROPE_STATUS_MODEL_ERROR;
    return false;
  }

  g_neai_rope_model_ready = 1U;
  g_neai_rope_status = NEAI_ROPE_STATUS_OK;
  NeaiRopeClassifier_Reset();
  return true;
}

void NeaiRopeClassifier_Reset(void)
{
  memset(neai_rope_sample_window, 0, sizeof(neai_rope_sample_window));
  memset(neai_rope_inference_input, 0, sizeof(neai_rope_inference_input));
  memset(neai_rope_probabilities, 0, sizeof(neai_rope_probabilities));
  neai_rope_sample_count = 0U;
  neai_rope_samples_since_inference = 0U;
  neai_rope_streak = 0U;
  neai_rope_still_streak = 0U;
  g_neai_rope_inference_count = 0U;
  g_neai_rope_probability_x1000 = 0U;
  g_neai_rope_still_probability_x1000 = 0U;
  g_neai_rope_window_samples = 0U;
  g_neai_rope_predicted_class = -1;
  g_neai_rope_has_prediction = 0U;
  g_neai_rope_active = 0U;
}

void NeaiRopeClassifier_PushSample(const float accel_g[3],
                                   const float gyro_rad_s[3])
{
  uint16_t base;

  if ((g_neai_rope_model_ready == 0U) || (accel_g == NULL) ||
      (gyro_rad_s == NULL))
  {
    return;
  }

  if (neai_rope_sample_count < NEAI_ROPE_WINDOW_SAMPLES)
  {
    base = neai_rope_sample_count * NEAI_ROPE_AXIS_COUNT;
    neai_rope_sample_count++;
    g_neai_rope_window_samples = neai_rope_sample_count;
  }
  else
  {
    memmove(neai_rope_sample_window,
            &neai_rope_sample_window[NEAI_ROPE_AXIS_COUNT],
            (NEAI_ROPE_SIGNAL_VALUES - NEAI_ROPE_AXIS_COUNT) *
            sizeof(float));
    base = NEAI_ROPE_SIGNAL_VALUES - NEAI_ROPE_AXIS_COUNT;
  }

  neai_rope_sample_window[base] = accel_g[0];
  neai_rope_sample_window[base + 1U] = accel_g[1];
  neai_rope_sample_window[base + 2U] = accel_g[2];
  neai_rope_sample_window[base + 3U] = gyro_rad_s[0];
  neai_rope_sample_window[base + 4U] = gyro_rad_s[1];
  neai_rope_sample_window[base + 5U] = gyro_rad_s[2];

  if (neai_rope_sample_count < NEAI_ROPE_WINDOW_SAMPLES)
  {
    return;
  }
  neai_rope_samples_since_inference++;
  if ((g_neai_rope_has_prediction == 0U) ||
      (neai_rope_samples_since_inference >= NEAI_ROPE_STRIDE_SAMPLES))
  {
    neai_rope_samples_since_inference = 0U;
    NeaiRopeClassifier_RunInference();
  }
}

bool NeaiRopeClassifier_AllowMetrics(void)
{
  return (g_neai_rope_model_ready != 0U) &&
         (g_neai_rope_has_prediction != 0U) &&
         (g_neai_rope_active != 0U);
}

uint8_t NeaiRopeClassifier_GetUiState(void)
{
  if ((g_neai_rope_status == NEAI_ROPE_STATUS_MODEL_ERROR) ||
      (g_neai_rope_status == NEAI_ROPE_STATUS_CONFIG_ERROR))
  {
    return (uint8_t)NEAI_ROPE_UI_ERROR;
  }
  if ((g_neai_rope_model_ready == 0U) ||
      (g_neai_rope_has_prediction == 0U))
  {
    return (uint8_t)NEAI_ROPE_UI_WAIT;
  }
  return (g_neai_rope_active != 0U) ? (uint8_t)NEAI_ROPE_UI_ROPE
                                    : (uint8_t)NEAI_ROPE_UI_STILL;
}
