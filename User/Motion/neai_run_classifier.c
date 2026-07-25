#include "neai_run_classifier.h"

#include <stddef.h>
#include <string.h>

#include "NanoEdgeAI_Run.h"

#define NEAI_RUN_AXIS_COUNT 6U
#define NEAI_RUN_WINDOW_SAMPLES 64U
#define NEAI_RUN_SIGNAL_VALUES \
  (NEAI_RUN_AXIS_COUNT * NEAI_RUN_WINDOW_SAMPLES)
#define NEAI_RUN_STRIDE_SAMPLES 16U
#define NEAI_RUN_CONFIDENCE_THRESHOLD 0.65f
#define NEAI_RUN_CONFIRMATIONS 2U

#define NEAI_RUN_STATUS_NOT_INITIALIZED 0x4E520000UL
#define NEAI_RUN_STATUS_OK 0x4E524F4BUL
#define NEAI_RUN_STATUS_MODEL_ERROR 0x4E524552UL
#define NEAI_RUN_STATUS_CONFIG_ERROR 0x4E524346UL

_Static_assert(RUN_NEAI_INPUT_SIGNAL_LENGTH == NEAI_RUN_WINDOW_SAMPLES,
               "RUN NanoEdge signal length mismatch");
_Static_assert(RUN_NEAI_INPUT_AXIS_NUMBER == NEAI_RUN_AXIS_COUNT,
               "RUN NanoEdge axis count mismatch");
_Static_assert(RUN_NEAI_NUMBER_OF_CLASSES == 2,
               "RUN NanoEdge class count mismatch");

volatile uint32_t g_neai_run_status = NEAI_RUN_STATUS_NOT_INITIALIZED;
volatile uint32_t g_neai_run_inference_count = 0U;
volatile uint16_t g_neai_run_probability_x1000 = 0U;
volatile uint16_t g_neai_run_still_probability_x1000 = 0U;
volatile uint16_t g_neai_run_window_samples = 0U;
volatile int8_t g_neai_run_predicted_class = -1;
volatile int8_t g_neai_run_class_id = -1;
volatile int8_t g_neai_run_still_class_id = -1;
volatile uint8_t g_neai_run_last_state =
  (uint8_t)RUN_NEAI_NOT_INITIALIZED;
volatile uint8_t g_neai_run_model_ready = 0U;
volatile uint8_t g_neai_run_has_prediction = 0U;
volatile uint8_t g_neai_run_active = 0U;

static float neai_run_sample_window[NEAI_RUN_SIGNAL_VALUES];
static float neai_run_inference_input[NEAI_RUN_SIGNAL_VALUES];
static float neai_run_probabilities[RUN_NEAI_NUMBER_OF_CLASSES];
static uint16_t neai_run_sample_count = 0U;
static uint16_t neai_run_samples_since_inference = 0U;
static uint8_t neai_run_streak = 0U;
static uint8_t neai_run_still_streak = 0U;

static uint8_t NeaiRunClassifier_NameStartsWith(const char* name,
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

static uint16_t NeaiRunClassifier_ProbabilityX1000(float probability)
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

static void NeaiRunClassifier_RunInference(void)
{
  RunNeaiState state;
  int predicted_class = -1;
  float predicted_probability;
  uint8_t raw_run;

  memcpy(neai_run_inference_input, neai_run_sample_window,
         sizeof(neai_run_inference_input));
  state = runmodel_neai_classification(neai_run_inference_input,
                                       neai_run_probabilities,
                                       &predicted_class);
  g_neai_run_last_state = (uint8_t)state;
  g_neai_run_inference_count++;
  if ((state != RUN_NEAI_OK) || (predicted_class < 0) ||
      (predicted_class >= (int)RUN_NEAI_NUMBER_OF_CLASSES))
  {
    g_neai_run_status = NEAI_RUN_STATUS_MODEL_ERROR;
    g_neai_run_model_ready = 0U;
    g_neai_run_active = 0U;
    return;
  }

  g_neai_run_predicted_class = (int8_t)predicted_class;
  g_neai_run_has_prediction = 1U;
  g_neai_run_probability_x1000 =
    NeaiRunClassifier_ProbabilityX1000(
      neai_run_probabilities[(uint8_t)g_neai_run_class_id]);
  g_neai_run_still_probability_x1000 =
    NeaiRunClassifier_ProbabilityX1000(
      neai_run_probabilities[(uint8_t)g_neai_run_still_class_id]);
  predicted_probability = neai_run_probabilities[(uint8_t)predicted_class];
  raw_run = ((predicted_class == g_neai_run_class_id) &&
             (predicted_probability >=
              NEAI_RUN_CONFIDENCE_THRESHOLD)) ? 1U : 0U;

  if (raw_run != 0U)
  {
    neai_run_still_streak = 0U;
    if (neai_run_streak < NEAI_RUN_CONFIRMATIONS)
    {
      neai_run_streak++;
    }
    if (neai_run_streak >= NEAI_RUN_CONFIRMATIONS)
    {
      g_neai_run_active = 1U;
    }
  }
  else
  {
    neai_run_streak = 0U;
    if (neai_run_still_streak < NEAI_RUN_CONFIRMATIONS)
    {
      neai_run_still_streak++;
    }
    if (neai_run_still_streak >= NEAI_RUN_CONFIRMATIONS)
    {
      g_neai_run_active = 0U;
    }
  }
  g_neai_run_status = NEAI_RUN_STATUS_OK;
}

bool NeaiRunClassifier_Init(void)
{
  RunNeaiState state;

  g_neai_run_status = NEAI_RUN_STATUS_NOT_INITIALIZED;
  g_neai_run_model_ready = 0U;
  g_neai_run_class_id = -1;
  g_neai_run_still_class_id = -1;

  if ((runmodel_neai_get_input_signal_size() !=
       (int)NEAI_RUN_WINDOW_SAMPLES) ||
      (runmodel_neai_get_axis_number() !=
       (int)NEAI_RUN_AXIS_COUNT) ||
      (runmodel_neai_get_number_of_classes() !=
       (int)RUN_NEAI_NUMBER_OF_CLASSES))
  {
    g_neai_run_status = NEAI_RUN_STATUS_CONFIG_ERROR;
    return false;
  }

  for (int class_id = 0;
       class_id < (int)RUN_NEAI_NUMBER_OF_CLASSES;
       class_id++)
  {
    const char* class_name = runmodel_neai_get_class_name(class_id);

    if (NeaiRunClassifier_NameStartsWith(class_name, "run") != 0U)
    {
      g_neai_run_class_id = (int8_t)class_id;
    }
    else if (NeaiRunClassifier_NameStartsWith(class_name, "still") != 0U)
    {
      g_neai_run_still_class_id = (int8_t)class_id;
    }
  }
  /*
   * Studio 5.2 metadata preserves the ordered labels (still, run), but this
   * generated archive exposes the class names as the numeric strings "0" and
   * "1". Accept that generated representation without changing the existing
   * WALK model or relying on a predicted-class hardcode during inference.
   */
  if ((g_neai_run_class_id < 0) && (g_neai_run_still_class_id < 0) &&
      (strcmp(runmodel_neai_get_class_name(0), "0") == 0) &&
      (strcmp(runmodel_neai_get_class_name(1), "1") == 0))
  {
    g_neai_run_still_class_id = 0;
    g_neai_run_class_id = 1;
  }
  if ((g_neai_run_class_id < 0) || (g_neai_run_still_class_id < 0))
  {
    g_neai_run_status = NEAI_RUN_STATUS_CONFIG_ERROR;
    return false;
  }

  state = runmodel_neai_classification_init();
  g_neai_run_last_state = (uint8_t)state;
  if (state != RUN_NEAI_OK)
  {
    g_neai_run_status = NEAI_RUN_STATUS_MODEL_ERROR;
    return false;
  }

  g_neai_run_model_ready = 1U;
  g_neai_run_status = NEAI_RUN_STATUS_OK;
  NeaiRunClassifier_Reset();
  return true;
}

void NeaiRunClassifier_Reset(void)
{
  memset(neai_run_sample_window, 0, sizeof(neai_run_sample_window));
  memset(neai_run_inference_input, 0, sizeof(neai_run_inference_input));
  memset(neai_run_probabilities, 0, sizeof(neai_run_probabilities));
  neai_run_sample_count = 0U;
  neai_run_samples_since_inference = 0U;
  neai_run_streak = 0U;
  neai_run_still_streak = 0U;
  g_neai_run_inference_count = 0U;
  g_neai_run_probability_x1000 = 0U;
  g_neai_run_still_probability_x1000 = 0U;
  g_neai_run_window_samples = 0U;
  g_neai_run_predicted_class = -1;
  g_neai_run_has_prediction = 0U;
  g_neai_run_active = 0U;
}

void NeaiRunClassifier_PushSample(const float accel_g[3],
                                  const float gyro_rad_s[3])
{
  uint16_t base;

  if ((g_neai_run_model_ready == 0U) || (accel_g == NULL) ||
      (gyro_rad_s == NULL))
  {
    return;
  }

  if (neai_run_sample_count < NEAI_RUN_WINDOW_SAMPLES)
  {
    base = neai_run_sample_count * NEAI_RUN_AXIS_COUNT;
    neai_run_sample_count++;
    g_neai_run_window_samples = neai_run_sample_count;
  }
  else
  {
    memmove(neai_run_sample_window,
            &neai_run_sample_window[NEAI_RUN_AXIS_COUNT],
            (NEAI_RUN_SIGNAL_VALUES - NEAI_RUN_AXIS_COUNT) *
            sizeof(float));
    base = NEAI_RUN_SIGNAL_VALUES - NEAI_RUN_AXIS_COUNT;
  }

  neai_run_sample_window[base] = accel_g[0];
  neai_run_sample_window[base + 1U] = accel_g[1];
  neai_run_sample_window[base + 2U] = accel_g[2];
  neai_run_sample_window[base + 3U] = gyro_rad_s[0];
  neai_run_sample_window[base + 4U] = gyro_rad_s[1];
  neai_run_sample_window[base + 5U] = gyro_rad_s[2];

  if (neai_run_sample_count < NEAI_RUN_WINDOW_SAMPLES)
  {
    return;
  }
  neai_run_samples_since_inference++;
  if ((g_neai_run_has_prediction == 0U) ||
      (neai_run_samples_since_inference >= NEAI_RUN_STRIDE_SAMPLES))
  {
    neai_run_samples_since_inference = 0U;
    NeaiRunClassifier_RunInference();
  }
}

bool NeaiRunClassifier_AllowMetrics(void)
{
  return (g_neai_run_model_ready != 0U) &&
         (g_neai_run_has_prediction != 0U) &&
         (g_neai_run_active != 0U);
}

uint8_t NeaiRunClassifier_GetUiState(void)
{
  if ((g_neai_run_status == NEAI_RUN_STATUS_MODEL_ERROR) ||
      (g_neai_run_status == NEAI_RUN_STATUS_CONFIG_ERROR))
  {
    return (uint8_t)NEAI_RUN_UI_ERROR;
  }
  if ((g_neai_run_model_ready == 0U) ||
      (g_neai_run_has_prediction == 0U))
  {
    return (uint8_t)NEAI_RUN_UI_WAIT;
  }
  return (g_neai_run_active != 0U) ? (uint8_t)NEAI_RUN_UI_RUN
                                   : (uint8_t)NEAI_RUN_UI_STILL;
}
