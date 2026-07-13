#include "motion_log.h"

#include <stddef.h>
#include <string.h>

#include "eeprom.h"

#define MOTION_LOG_MAGIC 0xA55AU
#define MOTION_LOG_VERSION 1U
#define MOTION_LOG_RECORD_SIZE 32U
#define MOTION_LOG_CAPACITY (EEPROM_CAPACITY_BYTES / MOTION_LOG_RECORD_SIZE)

#define MOTION_LOG_STATUS_NOT_INITIALIZED 0x4D4C0000UL
#define MOTION_LOG_STATUS_OK 0x4D4C4F4BUL
#define MOTION_LOG_STATUS_EEPROM_UNAVAILABLE 0x4D4C4E46UL
#define MOTION_LOG_STATUS_IO_ERROR 0x4D4C4552UL

typedef __PACKED_STRUCT
{
  uint16_t magic;
  uint8_t version;
  uint8_t sport;
  uint32_t sequence;
  uint32_t duration_s;
  uint32_t steps;
  uint32_t distance_cm;
  uint16_t average_speed_mmps;
  uint16_t cadence_x10;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t crc8;
} MotionLogStoredRecord;

_Static_assert(sizeof(MotionLogStoredRecord) == MOTION_LOG_RECORD_SIZE,
               "Motion log record must match one EEPROM page");

volatile uint32_t g_motion_log_status = MOTION_LOG_STATUS_NOT_INITIALIZED;
volatile uint16_t g_motion_log_count = 0U;
volatile uint32_t g_motion_log_next_sequence = 1UL;
volatile uint8_t g_motion_log_latest_valid = 0U;
volatile uint8_t g_motion_log_latest_raw[MOTION_LOG_EXPORT_RECORD_SIZE];

static uint16_t motion_log_next_slot = 0U;
static bool motion_log_ready = false;

static uint8_t MotionLog_Crc8(const uint8_t* data, uint16_t length)
{
  uint8_t crc = 0U;

  while (length-- > 0U)
  {
    crc ^= *data++;
    for (uint8_t bit = 0U; bit < 8U; bit++)
    {
      crc = ((crc & 0x80U) != 0U) ? (uint8_t)((crc << 1U) ^ 0x07U)
                                  : (uint8_t)(crc << 1U);
    }
  }

  return crc;
}

static bool MotionLog_RecordIsValid(const MotionLogStoredRecord* record)
{
  if ((record->magic != MOTION_LOG_MAGIC) ||
      (record->version != MOTION_LOG_VERSION) ||
      (record->sequence == 0U))
  {
    return false;
  }

  return MotionLog_Crc8((const uint8_t*)record,
                        MOTION_LOG_RECORD_SIZE - 1U) == record->crc8;
}

static void MotionLog_PublishRecord(const MotionLogStoredRecord* record)
{
  const uint8_t* source = (const uint8_t*)record;

  for (uint16_t index = 0U; index < MOTION_LOG_EXPORT_RECORD_SIZE; index++)
  {
    g_motion_log_latest_raw[index] = source[index];
  }
  g_motion_log_latest_valid = 1U;
}

static void MotionLog_ClearPublishedRecord(void)
{
  g_motion_log_latest_valid = 0U;
  for (uint16_t index = 0U; index < MOTION_LOG_EXPORT_RECORD_SIZE; index++)
  {
    g_motion_log_latest_raw[index] = 0U;
  }
}

static bool MotionLog_ReadSlot(uint16_t slot, MotionLogStoredRecord* record)
{
  if ((slot >= MOTION_LOG_CAPACITY) || (record == NULL))
  {
    return false;
  }

  return EEPROM_Read((uint16_t)(slot * MOTION_LOG_RECORD_SIZE),
                     record, sizeof(*record));
}

static void MotionLog_Decode(const MotionLogStoredRecord* stored,
                             MotionLogEntry* entry)
{
  entry->sequence = stored->sequence;
  entry->duration_s = stored->duration_s;
  entry->steps = stored->steps;
  entry->distance_cm = stored->distance_cm;
  entry->average_speed_mmps = stored->average_speed_mmps;
  entry->cadence_x10 = stored->cadence_x10;
  entry->sport = stored->sport;
  entry->end_time.year = stored->year;
  entry->end_time.month = stored->month;
  entry->end_time.day = stored->day;
  entry->end_time.hour = stored->hour;
  entry->end_time.minute = stored->minute;
  entry->end_time.second = stored->second;
}

bool MotionLog_Init(void)
{
  MotionLogStoredRecord record;
  uint32_t newest_sequence = 0U;
  uint16_t newest_slot = 0U;
  uint16_t valid_count = 0U;

  motion_log_ready = false;
  motion_log_next_slot = 0U;
  MotionLog_ClearPublishedRecord();
  g_motion_log_count = 0U;
  g_motion_log_next_sequence = 1U;

  if (!EEPROM_IsReady())
  {
    g_motion_log_status = MOTION_LOG_STATUS_EEPROM_UNAVAILABLE;
    return false;
  }

  for (uint16_t slot = 0U; slot < MOTION_LOG_CAPACITY; slot++)
  {
    if (!MotionLog_ReadSlot(slot, &record))
    {
      g_motion_log_status = MOTION_LOG_STATUS_IO_ERROR;
      return false;
    }
    if (MotionLog_RecordIsValid(&record))
    {
      valid_count++;
      if ((newest_sequence == 0U) ||
          ((int32_t)(record.sequence - newest_sequence) > 0))
      {
        newest_sequence = record.sequence;
        newest_slot = slot;
      }
    }
  }

  if (valid_count > MOTION_LOG_CAPACITY)
  {
    valid_count = MOTION_LOG_CAPACITY;
  }
  g_motion_log_count = valid_count;
  if (newest_sequence != 0U)
  {
    motion_log_next_slot = (uint16_t)((newest_slot + 1U) % MOTION_LOG_CAPACITY);
    g_motion_log_next_sequence = newest_sequence + 1U;
    if (g_motion_log_next_sequence == 0U)
    {
      g_motion_log_next_sequence = 1U;
    }
    if ((!MotionLog_ReadSlot(newest_slot, &record)) ||
        (!MotionLog_RecordIsValid(&record)))
    {
      g_motion_log_status = MOTION_LOG_STATUS_IO_ERROR;
      return false;
    }
    MotionLog_PublishRecord(&record);
  }

  motion_log_ready = true;
  g_motion_log_status = MOTION_LOG_STATUS_OK;
  return true;
}

bool MotionLog_Append(const MotionLogEntry* entry)
{
  MotionLogStoredRecord stored;
  MotionLogStoredRecord verify;
  const uint16_t address = (uint16_t)(motion_log_next_slot * MOTION_LOG_RECORD_SIZE);

  if ((!motion_log_ready) || (entry == NULL))
  {
    return false;
  }

  memset(&stored, 0, sizeof(stored));
  stored.magic = MOTION_LOG_MAGIC;
  stored.version = MOTION_LOG_VERSION;
  stored.sport = entry->sport;
  stored.sequence = g_motion_log_next_sequence;
  stored.duration_s = entry->duration_s;
  stored.steps = entry->steps;
  stored.distance_cm = entry->distance_cm;
  stored.average_speed_mmps = entry->average_speed_mmps;
  stored.cadence_x10 = entry->cadence_x10;
  stored.year = entry->end_time.year;
  stored.month = entry->end_time.month;
  stored.day = entry->end_time.day;
  stored.hour = entry->end_time.hour;
  stored.minute = entry->end_time.minute;
  stored.second = entry->end_time.second;
  stored.crc8 = MotionLog_Crc8((const uint8_t*)&stored,
                               MOTION_LOG_RECORD_SIZE - 1U);

  if ((!EEPROM_Write(address, &stored, sizeof(stored))) ||
      (!EEPROM_Read(address, &verify, sizeof(verify))) ||
      (memcmp(&stored, &verify, sizeof(stored)) != 0))
  {
    g_motion_log_status = MOTION_LOG_STATUS_IO_ERROR;
    return false;
  }

  motion_log_next_slot = (uint16_t)((motion_log_next_slot + 1U) % MOTION_LOG_CAPACITY);
  g_motion_log_next_sequence++;
  if (g_motion_log_next_sequence == 0U)
  {
    g_motion_log_next_sequence = 1U;
  }
  if (g_motion_log_count < MOTION_LOG_CAPACITY)
  {
    g_motion_log_count++;
  }
  MotionLog_PublishRecord(&stored);
  g_motion_log_status = MOTION_LOG_STATUS_OK;
  return true;
}

bool MotionLog_ReadNewest(uint16_t newest_index, MotionLogEntry* entry)
{
  MotionLogStoredRecord stored;
  uint16_t slot;

  if ((!motion_log_ready) || (entry == NULL) ||
      (newest_index >= g_motion_log_count))
  {
    return false;
  }

  slot = (uint16_t)((motion_log_next_slot + MOTION_LOG_CAPACITY - 1U - newest_index) %
                    MOTION_LOG_CAPACITY);
  if ((!MotionLog_ReadSlot(slot, &stored)) || (!MotionLog_RecordIsValid(&stored)))
  {
    g_motion_log_status = MOTION_LOG_STATUS_IO_ERROR;
    return false;
  }

  MotionLog_Decode(&stored, entry);
  return true;
}

uint16_t MotionLog_Count(void)
{
  return g_motion_log_count;
}
