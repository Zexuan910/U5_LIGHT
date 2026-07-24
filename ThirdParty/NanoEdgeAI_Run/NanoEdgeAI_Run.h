#ifndef NANOEDGEAI_RUN_H
#define NANOEDGEAI_RUN_H

#define RUN_NEAI_INPUT_SIGNAL_LENGTH 64
#define RUN_NEAI_INPUT_AXIS_NUMBER 6
#define RUN_NEAI_NUMBER_OF_CLASSES 2

typedef enum
{
  RUN_NEAI_OK = 0,
  RUN_NEAI_ERROR = 1,
  RUN_NEAI_NOT_INITIALIZED = 2,
  RUN_NEAI_INVALID_PARAM = 3,
  RUN_NEAI_NOT_SUPPORTED = 4,
  RUN_NEAI_LEARNING_DONE = 5,
  RUN_NEAI_LEARNING_IN_PROGRESS = 6
} RunNeaiState;

#ifdef __cplusplus
extern "C" {
#endif

RunNeaiState runmodel_neai_classification_init(void);
RunNeaiState runmodel_neai_classification(float* input,
                                          float* probabilities,
                                          int* class_id);
char* runmodel_neai_get_id(void);
int runmodel_neai_get_input_signal_size(void);
int runmodel_neai_get_axis_number(void);
int runmodel_neai_get_number_of_classes(void);
const char* runmodel_neai_get_class_name(int class_id);

#ifdef __cplusplus
}
#endif

#endif
