#ifndef NANOEDGEAI_ROPE_H
#define NANOEDGEAI_ROPE_H

#define ROPE_NEAI_INPUT_SIGNAL_LENGTH 64
#define ROPE_NEAI_INPUT_AXIS_NUMBER 6
#define ROPE_NEAI_NUMBER_OF_CLASSES 2

typedef enum
{
  ROPE_NEAI_OK = 0,
  ROPE_NEAI_ERROR = 1,
  ROPE_NEAI_NOT_INITIALIZED = 2,
  ROPE_NEAI_INVALID_PARAM = 3,
  ROPE_NEAI_NOT_SUPPORTED = 4,
  ROPE_NEAI_LEARNING_DONE = 5,
  ROPE_NEAI_LEARNING_IN_PROGRESS = 6
} RopeNeaiState;

#ifdef __cplusplus
extern "C" {
#endif

RopeNeaiState ropemodel_neai_classification_init(void);
RopeNeaiState ropemodel_neai_classification(float* input,
                                            float* probabilities,
                                            int* class_id);
char* ropemodel_neai_get_id(void);
int ropemodel_neai_get_input_signal_size(void);
int ropemodel_neai_get_axis_number(void);
int ropemodel_neai_get_number_of_classes(void);
const char* ropemodel_neai_get_class_name(int class_id);

#ifdef __cplusplus
}
#endif

#endif
