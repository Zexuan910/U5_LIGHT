#ifndef MOTION_LOG_H
#define MOTION_LOG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_LOG_SPORT_WALK 0U
#define MOTION_LOG_EXPORT_RECORD_SIZE 32U

typedef struct
{
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} MotionLogDateTime;

typedef struct
{
  uint32_t sequence;
  uint32_t duration_s;
  uint32_t steps;
  uint32_t distance_cm;
  uint16_t average_speed_mmps;
  uint16_t cadence_x10;
  uint8_t sport;
  MotionLogDateTime end_time;
} MotionLogEntry;

extern volatile uint32_t g_motion_log_status;
extern volatile uint16_t g_motion_log_count;
extern volatile uint32_t g_motion_log_next_sequence;
extern volatile uint8_t g_motion_log_latest_valid;
extern volatile uint8_t g_motion_log_latest_raw[MOTION_LOG_EXPORT_RECORD_SIZE];

bool MotionLog_Init(void);
bool MotionLog_Append(const MotionLogEntry* entry);
bool MotionLog_ReadNewest(uint16_t newest_index, MotionLogEntry* entry);
uint16_t MotionLog_Count(void);

#ifdef __cplusplus
}
#endif

#endif
